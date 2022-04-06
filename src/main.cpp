#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <learnopengl/filesystem.h>
#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>
#include <iostream>

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
unsigned int loadCubemap(vector<std::string> faces);
void renderQuad();

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
// camera
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

struct PointLight {
    glm::vec3 position;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;

    float constant;
    float linear;
    float quadratic;
};
struct ProgramState {
    glm::vec3 clearColor = glm::vec3(0);
    bool ImGuiEnabled = false;
    Camera camera;
    bool CameraMouseMovementUpdateEnabled = true;
    PointLight pointLight;
    ProgramState()
            : camera(glm::vec3(0.0f, 0.0f, 3.0f)) {}
    void SaveToFile(std::string filename);
    void LoadFromFile(std::string filename);
};
void ProgramState::SaveToFile(std::string filename) {
    std::ofstream out(filename);
    out << clearColor.r << '\n'
        << clearColor.g << '\n'
        << clearColor.b << '\n'
        << ImGuiEnabled << '\n'
        << camera.Position.x << '\n'
        << camera.Position.y << '\n'
        << camera.Position.z << '\n'
        << camera.Front.x << '\n'
        << camera.Front.y << '\n'
        << camera.Front.z << '\n';
}
void ProgramState::LoadFromFile(std::string filename) {
    std::ifstream in(filename);
    if (in) {
        in >> clearColor.r
           >> clearColor.g
           >> clearColor.b
           >> ImGuiEnabled
           >> camera.Position.x
           >> camera.Position.y
           >> camera.Position.z
           >> camera.Front.x
           >> camera.Front.y
           >> camera.Front.z;
    }
}
ProgramState *programState;
void DrawImGui(ProgramState *programState);
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    // glfw window creation
    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);
    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    programState = new ProgramState;
    programState->LoadFromFile("resources/program_state.txt");
    if (programState->ImGuiEnabled) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    // Init Imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // configure global opengl state
    glEnable(GL_DEPTH_TEST);
    //blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //Face cull
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glFrontFace(GL_CW);

    // ----------------------------build and compile shaders--------------------------------------------
    Shader ourShader("resources/shaders/mian.vs", "resources/shaders/main.fs");
    Shader blendingShader("resources/shaders/mian.vs", "resources/shaders/blending.fs");
    Shader skyboxShader("resources/shaders/skybox.vs", "resources/shaders/skybox.fs");
    Shader shaderBlur("resources/shaders/blur.vs", "resources/shaders/blur.fs");
    Shader shaderBloomFinal("resources/shaders/bloom_final.vs", "resources/shaders/bloom_final.fs");
    // --------------------------------load models-----------------------------------------------
    //load windmills
    Model windmillModel("resources/objects/main/Papercraft Windmills.obj");
    windmillModel.SetShaderTextureNamePrefix("material.");
    //load island
    Model islandModel("resources/objects/island/island1_design2_c4d.obj");
    islandModel.SetShaderTextureNamePrefix("material.");
    //load sun
    Model sunModel("resources/objects/sun/sun.obj");
    sunModel.SetShaderTextureNamePrefix("material.");

    //------------------------------------bloom-------------------------------------------------
    // configure framebuffers
    unsigned int hdrFBO;
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    unsigned int colorBuffers[2];
    glGenTextures(2, colorBuffers);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);  // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
    }
    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBuffers[0], 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);

    // check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ping-pong-framebuffer for blurring
    unsigned int pingpongFBO[2];
    unsigned int pingpongColorbuffers[2];
    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongColorbuffers);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "Framebuffer not complete!" << std::endl;
    }
    //_______________________________________skybox vertices_____________________________________________________
    float skyboxVertices[] = {
            // positions
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f,  1.0f
    };
    //______________________________________skybox VAO_________________________________________________________
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    // ______________________________________load textures for skybox________________________________________________________
    vector<std::string> faces =
            {
                    FileSystem::getPath("resources/textures/skybox/right.jpg"),
                    FileSystem::getPath("resources/textures/skybox/left.jpg"),
                    FileSystem::getPath("resources/textures/skybox/up.jpg"),
                    FileSystem::getPath("resources/textures/skybox/bottom.jpg"),
                    FileSystem::getPath("resources/textures/skybox/front.jpg"),
                    FileSystem::getPath("resources/textures/skybox/back.jpg"),
            };

    unsigned int cubemapTexture = loadCubemap(faces);

    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);

    PointLight& pointLight = programState->pointLight;
    pointLight.position = glm::vec3(4.0f, 4.0, 0.0);
    pointLight.ambient = glm::vec3(0.1, 0.1, 0.1);
    pointLight.diffuse = glm::vec3(0.6, 0.6, 0.6);
    pointLight.specular = glm::vec3(1.0, 1.0, 1.0);

    pointLight.constant = 0.1f;
    pointLight.linear = 1.0f;
    pointLight.quadratic = 1.0f;

    shaderBlur.use();
    shaderBlur.setInt("image", 0);
    shaderBloomFinal.use();
    shaderBloomFinal.setInt("scene", 0);
    shaderBloomFinal.setInt("bloomBlur", 1);

    // --------------------------------------render loop-----------------------------------
    while (!glfwWindowShouldClose(window)) {
        // per-frame time logic
        // --------------------
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        processInput(window);

        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // render
        glClearColor(programState->clearColor.r, programState->clearColor.g, programState->clearColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ourShader.use();
        ourShader.setVec3("viewPosition", programState->camera.Position);
        ourShader.setFloat("material.shininess", 32.0f);
        // view/projection transformations
        glm::mat4 projection = glm::perspective(glm::radians(programState->camera.Zoom),
                                                (float) SCR_WIDTH / (float) SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = programState->camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);

        //---------------------------------------Lights---------------------------------------------------------
        // directional light
        ourShader.setVec3("dirLight.direction", glm::vec3(-0.547f, -0.727f, 0.415f));
        ourShader.setVec3("dirLight.ambient", glm::vec3(0.35f));
        ourShader.setVec3("dirLight.diffuse", glm::vec3(0.4f));
        ourShader.setVec3("dirLight.specular", glm::vec3(0.2f));
        //pointlights
        ourShader.setVec3("pointLights[0].position", glm::vec3(6.7f+sin(currentFrame)*0.6f,1.15f+cos(currentFrame*2)*0.2f+sin(currentFrame)*0.4f,-17.45f+cos(currentFrame)*0.6f));
        ourShader.setVec3("pointLights[0].ambient", pointLight.ambient);
        ourShader.setVec3("pointLights[0].diffuse", pointLight.diffuse);
        ourShader.setVec3("pointLights[0].specular", pointLight.specular);
        ourShader.setFloat("pointLights[0].constant", pointLight.constant);
        ourShader.setFloat("pointLights[0].linear", pointLight.linear);
        ourShader.setFloat("pointLights[0].quadratic", pointLight.quadratic);

        ourShader.setVec3("pointLights[1].position", glm::vec3(6.7f+cos(currentFrame*2)*0.6f,1.15f+cos(currentFrame)*0.2f+cos(currentFrame)*0.4f,-17.45f+sin(currentFrame)*0.6f));
        ourShader.setVec3("pointLights[1].ambient", pointLight.ambient);
        ourShader.setVec3("pointLights[1].diffuse", pointLight.diffuse);
        ourShader.setVec3("pointLights[1].specular", pointLight.specular);
        ourShader.setFloat("pointLights[1].constant", pointLight.constant);
        ourShader.setFloat("pointLights[1].linear", pointLight.linear);
        ourShader.setFloat("pointLights[1].quadratic", pointLight.quadratic);

        //render island
        glDisable(GL_CULL_FACE);
        glm::mat4 modelIsland = glm::mat4(1.0f);
        modelIsland = glm::translate(modelIsland,glm::vec3(6.7f,1.6f+cos(currentFrame)*0.2f,-17.45f));
        modelIsland = glm::scale(modelIsland, glm::vec3(0.003f));
        modelIsland = glm::rotate(modelIsland,glm::radians(currentFrame*20), glm::vec3(0.0f ,1.0f, 0.0f));
        ourShader.setMat4("model", modelIsland);
        islandModel.Draw(ourShader);
        glEnable(GL_CULL_FACE);
        //render big island
        modelIsland = glm::mat4(1.0f);
        modelIsland = glm::translate(modelIsland,glm::vec3(5.56f,2.0f+sin(currentFrame),-55.45f));
        modelIsland = glm::scale(modelIsland, glm::vec3(0.1f));
        modelIsland = glm::rotate(modelIsland,glm::radians(15.0f), glm::vec3(1.0f,0.0f,0.0f));
        modelIsland = glm::rotate(modelIsland,glm::radians(-180.0f), glm::vec3(0.0f ,1.0f, 0.0f));
        ourShader.setMat4("model", modelIsland);
        islandModel.Draw(ourShader);
        //render big island2
        modelIsland = glm::mat4(1.0f);
        modelIsland = glm::translate(modelIsland,glm::vec3(6.55f,15.5f+sin(currentFrame),25.45f));
        modelIsland = glm::scale(modelIsland, glm::vec3(0.15f));
        modelIsland = glm::rotate(modelIsland,glm::radians(-14.0f), glm::vec3(1.0f,0.0f,0.0f));
        modelIsland = glm::rotate(modelIsland,glm::radians(50.0f), glm::vec3(0.0f ,1.0f, 0.0f));
        ourShader.setMat4("model", modelIsland);
        islandModel.Draw(ourShader);
        //render windmills
        glm::mat4 modelWindmill = glm::mat4(1.0f);
        modelWindmill = glm::scale(modelWindmill, glm::vec3(glm::vec3(0.02f)));
        ourShader.setMat4("model", modelWindmill);
        windmillModel.Draw(ourShader);

        //blending shader config
        blendingShader.use();
        blendingShader.setVec3("viewPosition", programState->camera.Position);
        blendingShader.setFloat("material.shininess", 32.0f);
        
        blendingShader.setMat4("projection", projection);
        blendingShader.setMat4("view", view);

        blendingShader.setVec3("dirLight.direction", glm::vec3(-0.547f, -0.727f, 0.415f));
        blendingShader.setVec3("dirLight.ambient", glm::vec3(0.35f));
        blendingShader.setVec3("dirLight.diffuse", glm::vec3(0.4f));
        blendingShader.setVec3("dirLight.specular", glm::vec3(0.2f));

        blendingShader.setVec3("pointLights[0].position", glm::vec3(6.7f+sin(currentFrame)*0.6f,1.6f+cos(currentFrame*2)*0.2f+sin(currentFrame)*0.4f,-17.45f+cos(currentFrame)*0.6f));
        blendingShader.setVec3("pointLights[0].ambient", pointLight.ambient);
        blendingShader.setVec3("pointLights[0].diffuse", pointLight.diffuse);
        blendingShader.setVec3("pointLights[0].specular", pointLight.specular);
        blendingShader.setFloat("pointLights[0].constant", pointLight.constant);
        blendingShader.setFloat("pointLights[0].linear", pointLight.linear);
        blendingShader.setFloat("pointLights[0].quadratic", pointLight.quadratic);

        blendingShader.setVec3("pointLights[1].position", glm::vec3(6.7f+cos(currentFrame*2)*0.6f,1.6f+cos(currentFrame)*0.2f+cos(currentFrame)*0.4f,-17.45f+sin(currentFrame)*0.6f));
        blendingShader.setVec3("pointLights[1].ambient", pointLight.ambient);
        blendingShader.setVec3("pointLights[1].diffuse", pointLight.diffuse);
        blendingShader.setVec3("pointLights[1].specular", pointLight.specular);
        blendingShader.setFloat("pointLights[1].constant", pointLight.constant);
        blendingShader.setFloat("pointLights[1].linear", pointLight.linear);
        blendingShader.setFloat("pointLights[1].quadratic", pointLight.quadratic);
        //render sun
        glDisable(GL_CULL_FACE);
        glm::mat4 modelSun= glm::mat4(1.0f);
        modelSun = glm::translate(modelSun,glm::vec3(6.7f+sin(currentFrame)*0.6f,1.6f+cos(currentFrame*2)*0.2f+sin(currentFrame)*0.4f,-17.45f+cos(currentFrame)*0.6f));
        modelSun = glm::scale(modelSun, glm::vec3(0.0008f));
        modelSun = glm::rotate(modelSun,glm::radians(90.0f), glm::vec3(1.0f,0.0f , 0.0f));
        modelSun = glm::rotate(modelSun,glm::radians(currentFrame*20), glm::vec3(0.0f ,1.0f, 0.0f));
        modelSun = glm::rotate(modelSun,glm::radians(currentFrame*40), glm::vec3(1.0f , 0.0f,0.0f));
        blendingShader.setMat4("model", modelSun);
        sunModel.Draw(blendingShader);
        //sun2
        modelSun= glm::mat4(1.0f);
        modelSun = glm::translate(modelSun,glm::vec3(6.7f+cos(currentFrame*2)*0.6f,1.6f+cos(currentFrame)*0.2f+cos(currentFrame)*0.4f,-17.45f+sin(currentFrame)*0.6f));
        modelSun = glm::scale(modelSun, glm::vec3(0.0008f));
        modelSun = glm::rotate(modelSun,glm::radians(90.0f), glm::vec3(1.0f,0.0f , 0.0f));
        modelSun = glm::rotate(modelSun,glm::radians(-currentFrame*20), glm::vec3(0.0f ,1.0f, 0.0f));
        modelSun = glm::rotate(modelSun,glm::radians(currentFrame*20), glm::vec3(1.0f , 0.0f,0.0f));
        blendingShader.setMat4("model", modelSun);
        sunModel.Draw(blendingShader);
        //______________________________________draw skybox_____________________________________________________
        glDepthFunc(GL_LEQUAL);
        skyboxShader.use();
        view = glm::mat4(glm::mat3(programState->camera.GetViewMatrix()));
        skyboxShader.setMat4("view", view);
        skyboxShader.setMat4("projection", projection);
        // skybox cube
        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS); // set depth function back to default
        // ---------------------------------blur--------------------------------------------------------
        bool horizontal = true, first_iteration = true;
        unsigned int amount = 5;
        shaderBlur.use();
        for (unsigned int i = 0; i < amount; i++)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
            shaderBlur.setInt("horizontal", horizontal);
            glBindTexture(GL_TEXTURE_2D, first_iteration ? colorBuffers[1] : pingpongColorbuffers[!horizontal]);  // bind texture of other framebuffer (or scene if first iteration)
            renderQuad();
            horizontal = !horizontal;
            if (first_iteration)
                first_iteration = false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        shaderBloomFinal.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[!horizontal]);
        shaderBloomFinal.setInt("bloom", true);
        shaderBloomFinal.setFloat("exposure", 1.0f);
        renderQuad();

        if (programState->ImGuiEnabled)
            DrawImGui(programState);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &skyboxVAO);

    programState->SaveToFile("resources/program_state.txt");
    delete programState;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}
void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(RIGHT, deltaTime);
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    glViewport(0, 0, width, height);
}
void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;

    lastX = xpos;
    lastY = ypos;

    if (programState->CameraMouseMovementUpdateEnabled)
        programState->camera.ProcessMouseMovement(xoffset, yoffset);
}
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    programState->camera.ProcessMouseScroll(yoffset);
}
void DrawImGui(ProgramState *programState) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    {
        static float f = 0.0f;
        ImGui::Begin("Settings");
        ImGui::Text("Point light settings:");
        ImGui::DragFloat("pointLight.constant", &programState->pointLight.constant, 0.005, 0.0001, 1.0);
        ImGui::DragFloat("pointLight.linear", &programState->pointLight.linear, 0.005, 0.0001, 1.0);
        ImGui::DragFloat("pointLight.quadratic", &programState->pointLight.quadratic, 0.005, 0.0001, 1.0);
        ImGui::End();
    }

    {
        ImGui::Begin("Camera info");
        const Camera& c = programState->camera;
        ImGui::Text("Camera position: (%f, %f, %f)", c.Position.x, c.Position.y, c.Position.z);
        ImGui::Text("(Yaw, Pitch): (%f, %f)", c.Yaw, c.Pitch);
        ImGui::Text("Camera front: (%f, %f, %f)", c.Front.x, c.Front.y, c.Front.z);
        ImGui::Checkbox("Camera mouse update", &programState->CameraMouseMovementUpdateEnabled);
        ImGui::End();
    }
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
        programState->ImGuiEnabled = !programState->ImGuiEnabled;
        if (programState->ImGuiEnabled) {
            programState->CameraMouseMovementUpdateEnabled = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            programState->CameraMouseMovementUpdateEnabled = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
}
unsigned int loadCubemap(vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrComponents;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrComponents, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    return textureID;
}
unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad()
{
    if (quadVAO == 0)
    {
        float quadVertices[] = {
                // positions        // texture Coords
                -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
                -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
                1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
                1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}