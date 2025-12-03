//     Universidade Federal do Rio Grande do Sul
//             Instituto de Informática
//       Departamento de Informática Aplicada
//
//    INF01047 Fundamentos de Computação Gráfica
//               Prof. Eduardo Gastal
//
//                   LABORATÓRIO 5
//

// Arquivos "headers" padrões de C podem ser incluídos em um
// programa C++, sendo necessário somente adicionar o caractere
// "c" antes de seu nome, e remover o sufixo ".h". Exemplo:
//    #include <stdio.h> // Em C
//  vira
//    #include <cstdio> // Em C++
//
#include <cmath>
#include <cstdio>
#include <cstdlib>

// Headers abaixo são específicos de C++
#include <set>
#include <map>
#include <stack>
#include <string>
#include <vector>
#include <limits>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

// Headers das bibliotecas OpenGL
#include <glad/glad.h>   // Criação de contexto OpenGL 3.3
#include <GLFW/glfw3.h>  // Criação de janelas do sistema operacional

// Headers da biblioteca GLM: criação de matrizes e vetores.
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/type_ptr.hpp>

// Headers da biblioteca para carregar modelos obj
#include <tiny_obj_loader.h>

#include <stb_image.h>

// Headers locais, definidos na pasta "include/"
#include "utils.h"
#include "matrices.h"
#include "collisions.h"

#define SKYBOX 0
#define AIRCRAFT 1
#define PLANE 2
#define ENEMY 3
#define CHECKPOINT_SPHERE 4
#define HEALTH_BAR_BACKGROUND 5
#define HEALTH_BAR_FOREGROUND 6
#define ASTEROID 7
#define MISSILE 8 

#define MAX_LIFE 3 
#define M_PI_2 1.57079632679489661923
#define M_PI 3.14159265358979323846

// Estrutura que representa um modelo geométrico carregado a partir de um
// arquivo ".obj". Veja https://en.wikipedia.org/wiki/Wavefront_.obj_file .
struct ObjModel
{
    tinyobj::attrib_t                 attrib;
    std::vector<tinyobj::shape_t>     shapes;
    std::vector<tinyobj::material_t>  materials;

    // Este construtor lê o modelo de um arquivo utilizando a biblioteca tinyobjloader.
    // Veja: https://github.com/syoyo/tinyobjloader
    ObjModel(const char* filename, const char* basepath = NULL, bool triangulate = true)
    {
        printf("Carregando objetos do arquivo \"%s\"...\n", filename);

        // Se basepath == NULL, então setamos basepath como o dirname do
        // filename, para que os arquivos MTL sejam corretamente carregados caso
        // estejam no mesmo diretório dos arquivos OBJ.
        std::string fullpath(filename);
        std::string dirname;
        if (basepath == NULL)
        {
            auto i = fullpath.find_last_of("/");
            if (i != std::string::npos)
            {
                dirname = fullpath.substr(0, i+1);
                basepath = dirname.c_str();
            }
        }

        std::string warn;
        std::string err;
        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename, basepath, triangulate);

        if (!err.empty())
            fprintf(stderr, "\n%s\n", err.c_str());

        if (!ret)
            throw std::runtime_error("Erro ao carregar modelo.");

        for (size_t shape = 0; shape < shapes.size(); ++shape)
        {
            if (shapes[shape].name.empty())
            {
                fprintf(stderr,
                        "*********************************************\n"
                        "Erro: Objeto sem nome dentro do arquivo '%s'.\n"
                        "Veja https://www.inf.ufrgs.br/~eslgastal/fcg-faq-etc.html#Modelos-3D-no-formato-OBJ .\n"
                        "*********************************************\n",
                    filename);
                throw std::runtime_error("Objeto sem nome.");
            }
            printf("- Objeto '%s'\n", shapes[shape].name.c_str());
        }

        printf("OK.\n");
    }
};

struct Enemy {
    glm::vec4 position;     // Posição no mundo
    glm::vec4 forward;      // Para onde ele está olhando/indo
    float speed;            // Velocidade de movimento
    float changeDirTimer;   // Tempo até a próxima mudança de direção aleatória
};

struct Missile {
    glm::vec4 position;     // Posição no mundo
    glm::vec4 forward;      // Direção do voo (tangente à órbita)
    float speed;            // Velocidade
    float fixedDistance;    // Distância fixa da órbita (16.0f)
    bool isActive;          // Se está ativo no jogo
    int ownerId;            // Quem disparou (0: Nave, 1: Inimigo ID)
    float lifeTime;         // Tempo de vida (para auto-destruição)
};

// Declaração de funções utilizadas para pilha de matrizes de modelagem.
void PushMatrix(glm::mat4 M);
void PopMatrix(glm::mat4& M);

// Declaração de várias funções utilizadas em main().  Essas estão definidas
// logo após a definição de main() neste arquivo.
void BuildTrianglesAndAddToVirtualScene(ObjModel*); // Constrói representação de um ObjModel como malha de triângulos para renderização
void ComputeNormals(ObjModel* model); // Computa normais de um ObjModel, caso não existam.
void LoadShadersFromFiles(); // Carrega os shaders de vértice e fragmento, criando um programa de GPU
void LoadTextureImage(const char* filename); // Função que carrega imagens de textura
void DrawVirtualObject(const char* object_name); // Desenha um objeto armazenado em g_VirtualScene
GLuint LoadShader_Vertex(const char* filename);   // Carrega um vertex shader
GLuint LoadShader_Fragment(const char* filename); // Carrega um fragment shader
void LoadShader(const char* filename, GLuint shader_id); // Função utilizada pelas duas acima
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id); // Cria um programa de GPU
void PrintObjModelInfo(ObjModel*); // Função para debugging

// Declaração de funções auxiliares para renderizar texto dentro da janela
// OpenGL. Estas funções estão definidas no arquivo "textrendering.cpp".
void TextRendering_Init();
float TextRendering_LineHeight(GLFWwindow* window);
float TextRendering_CharWidth(GLFWwindow* window);
void TextRendering_PrintString(GLFWwindow* window, const std::string &str, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrix(GLFWwindow* window, glm::mat4 M, float x, float y, float scale = 1.0f);
void TextRendering_PrintVector(GLFWwindow* window, glm::vec4 v, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrixVectorProduct(GLFWwindow* window, glm::mat4 M, glm::vec4 v, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrixVectorProductMoreDigits(GLFWwindow* window, glm::mat4 M, glm::vec4 v, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrixVectorProductDivW(GLFWwindow* window, glm::mat4 M, glm::vec4 v, float x, float y, float scale = 1.0f);

// Funções abaixo renderizam como texto na janela OpenGL algumas matrizes e
// outras informações do programa. Definidas após main().
void TextRendering_ShowModelViewProjection(GLFWwindow* window, glm::mat4 projection, glm::mat4 view, glm::mat4 model, glm::vec4 p_model);
void TextRendering_ShowEulerAngles(GLFWwindow* window);
void TextRendering_ShowProjection(GLFWwindow* window);
void TextRendering_ShowFramesPerSecond(GLFWwindow* window);

// Funções callback para comunicação com o sistema operacional e interação do
// usuário. Veja mais comentários nas definições das mesmas, abaixo.
void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
void ErrorCallback(int error, const char* description);
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mode);
void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
void moveAircraft(float tprev, float tnow, glm::vec4& aircraft_position);
float randomFloat(float min, float max);
void InitEnemies();
void moveEnemies(float tprev, float tnow);
glm::vec4 evaluateBezier(glm::vec4 p0, glm::vec4 p1, glm::vec4 p2, glm::vec4 p3, float t);
void processCollisions();
glm::vec4 normalizedVec(glm::vec4 v);
bool isGameOver();
void resetGame();
void showText(GLFWwindow* window);
void initCheckpoints();
void initRandomAsteroids();
void fireMissile(const glm::vec4& startPos, const glm::vec4& direction, int ownerId);
void updateMissiles(float tprev, float tnow);


// Definimos uma estrutura que armazenará dados necessários para renderizar
// cada objeto da cena virtual.
struct SceneObject
{
    std::string  name;        // Nome do objeto
    size_t       first_index; // Índice do primeiro vértice dentro do vetor indices[] definido em BuildTrianglesAndAddToVirtualScene()
    size_t       num_indices; // Número de índices do objeto dentro do vetor indices[] definido em BuildTrianglesAndAddToVirtualScene()
    GLenum       rendering_mode; // Modo de rasterização (GL_TRIANGLES, GL_TRIANGLE_STRIP, etc.)
    GLuint       vertex_array_object_id; // ID do VAO onde estão armazenados os atributos do modelo
    glm::vec3    bbox_min; // Axis-Aligned Bounding Box do objeto
    glm::vec3    bbox_max;
};

// Abaixo definimos variáveis globais utilizadas em várias funções do código.

// A cena virtual é uma lista de objetos nomeados, guardados em um dicionário
// (map).  Veja dentro da função BuildTrianglesAndAddToVirtualScene() como que são incluídos
// objetos dentro da variável g_VirtualScene, e veja na função main() como
// estes são acessados.
std::map<std::string, SceneObject> g_VirtualScene;

// Pilha que guardará as matrizes de modelagem.
std::stack<glm::mat4>  g_MatrixStack;

// Razão de proporção da janela (largura/altura). Veja função FramebufferSizeCallback().
float g_ScreenRatio = 1.0f;

// Ângulos de Euler que controlam a rotação de um dos cubos da cena virtual
float g_AngleX = 0.0f;
float g_AngleY = 0.0f;
float g_AngleZ = 0.0f;

// "g_LeftMouseButtonPressed = true" se o usuário está com o botão esquerdo do mouse
// pressionado no momento atual. Veja função MouseButtonCallback().
bool g_LeftMouseButtonPressed = false;
bool g_RightMouseButtonPressed = false; // Análogo para botão direito do mouse
bool g_MiddleMouseButtonPressed = false; // Análogo para botão do meio do mouse

// Variáveis que definem a câmera em coordenadas esféricas, controladas pelo
// usuário através do mouse (veja função CursorPosCallback()). A posição
// efetiva da câmera é calculada dentro da função main(), dentro do loop de
// renderização.
float g_CameraTheta = 0.0f; // Ângulo no plano ZX em relação ao eixo Z
float g_CameraPhi = -0.5f;   // Ângulo em relação ao eixo Y
float g_CameraDistance = 3.5f; // Distância da câmera para a origem

// Variáveis que controlam rotação do antebraço
float g_ForearmAngleZ = 0.0f;
float g_ForearmAngleX = 0.0f;

// Variáveis que controlam translação do torso
float g_TorsoPositionX = 0.0f;
float g_TorsoPositionY = 0.0f;

// Variável que controla o tipo de projeção utilizada: perspectiva ou ortográfica.
bool g_UsePerspectiveProjection = true;

// Variável que controla se o texto informativo será mostrado na tela.
bool g_ShowInfoText = true;

// Variáveis que definem um programa de GPU (shaders). Veja função LoadShadersFromFiles().
GLuint g_GpuProgramID = 0;
GLint g_model_uniform;
GLint g_view_uniform;
GLint g_projection_uniform;
GLint g_object_id_uniform;
GLint g_bbox_min_uniform;
GLint g_bbox_max_uniform;
GLint g_gouraud_uniform;
GLint g_is_damaged_uniform;

// Número de texturas carregadas pela função LoadTextureImage()
GLuint g_NumLoadedTextures = 0;

// Variáveis que controlam a posição da aeronave no mundo
glm::vec4 g_AircraftPosition = glm::vec4(0.0f, 0.0f, 16.0f, 1.0f);
glm::vec4 g_AircraftForward = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
glm::vec4 g_AircraftPosition_Prev = glm::vec4(g_AircraftPosition); // Inicializa com a posição inicial.

// Lista global de inimigos
std::vector<Enemy> g_Enemies;

// Flags para controle de movimento 
float isWPressed = false;
float isAPressed = false;
float isDPressed = false;
float isIPressed = false;

// Variaveis de posição da lua
glm::vec4 moon_position = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

std::vector<glm::vec4> g_Checkpoints; // Posições dos 5 Checkpoints

const float turnRate = 2.0f; // Velocidade máxima de giro 
const float enemySpeed = 5.0f; // Velocidade do inimigo

// Variaveis globais de controle da Câmera Livre
bool g_UseFirstPersonCamera = false; 

int g_AircraftLife = 3; 
bool g_IsGameOver = false;

glm::vec4 g_Asteroid_P0 = glm::vec4(21.0f, 0.0f, 0.0f, 1.0f);   
glm::vec4 g_Asteroid_P1 = glm::vec4(17.0f, 4.0f, 4.0f, 1.0f);   
glm::vec4 g_Asteroid_P2 = glm::vec4(25.0f, 4.0f, -4.0f, 1.0f); 
glm::vec4 g_Asteroid_P3 = glm::vec4(21.0f, 0.0f, 0.0f, 1.0f);    
float g_Asteroid_t = 0.0f;
const float asteroidSpeed = 0.50f; 
const float asteroidScale = 0.0004f; 

std::vector<glm::vec4> g_RandomAsteroids; 
const int numRandomAsteroids = 8; 

// Variável Global para armazenar todos os mísseis
std::vector<Missile> g_Missiles;
const float missileSpeed = 20.0f;
const float missileLifespan = 2.0f; // 2 segundos de vida
const float missileRadius = 0.15f; 

// Variáveis de controle de tiro da nave
float g_PlayerShotTimer = 0.0f;
const float shootColdownTimer = 1.0f; // Intervalo de 1s entre tiros

const float enemyShotSpeed = 7.0f; // Inimigo atira a cada 5 segundos

// Feedback de dano
float g_DamageTimer = 0.0f;
const float damageDuration = 0.2f; // Nave fica vermelha por 0.2 segundos

// Voo livre
bool g_FreeWorld = false;

int main(int argc, char* argv[])
{
    // Inicializamos a biblioteca GLFW, utilizada para criar uma janela do
    // sistema operacional, onde poderemos renderizar com OpenGL.
    int success = glfwInit();
    if (!success)
    {
        fprintf(stderr, "ERROR: glfwInit() failed.\n");
        std::exit(EXIT_FAILURE);
    }

    // Definimos o callback para impressão de erros da GLFW no terminal
    glfwSetErrorCallback(ErrorCallback);

    // Pedimos para utilizar OpenGL versão 3.3 (ou superior)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    #ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif

    // Pedimos para utilizar o perfil "core", isto é, utilizaremos somente as
    // funções modernas de OpenGL.
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Criamos uma janela do sistema operacional, com 800 colunas e 600 linhas
    // de pixels, e com título "INF01047 ...".
    GLFWwindow* window;
    window = glfwCreateWindow(800, 600, "INF01047 - Seu Cartao - Seu Nome", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        fprintf(stderr, "ERROR: glfwCreateWindow() failed.\n");
        std::exit(EXIT_FAILURE);
    }

    // Definimos a função de callback que será chamada sempre que o usuário
    // pressionar alguma tecla do teclado ...
    glfwSetKeyCallback(window, KeyCallback);
    // ... ou clicar os botões do mouse ...
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    // ... ou movimentar o cursor do mouse em cima da janela ...
    glfwSetCursorPosCallback(window, CursorPosCallback);
    // ... ou rolar a "rodinha" do mouse.
    glfwSetScrollCallback(window, ScrollCallback);

    // Indicamos que as chamadas OpenGL deverão renderizar nesta janela
    glfwMakeContextCurrent(window);

    // Carregamento de todas funções definidas por OpenGL 3.3, utilizando a
    // biblioteca GLAD.
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);

    // Definimos a função de callback que será chamada sempre que a janela for
    // redimensionada, por consequência alterando o tamanho do "framebuffer"
    // (região de memória onde são armazenados os pixels da imagem).
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    FramebufferSizeCallback(window, 800, 600); // Forçamos a chamada do callback acima, para definir g_ScreenRatio.

    // Imprimimos no terminal informações sobre a GPU do sistema
    const GLubyte *vendor      = glGetString(GL_VENDOR);
    const GLubyte *renderer    = glGetString(GL_RENDERER);
    const GLubyte *glversion   = glGetString(GL_VERSION);
    const GLubyte *glslversion = glGetString(GL_SHADING_LANGUAGE_VERSION);

    printf("GPU: %s, %s, OpenGL %s, GLSL %s\n", vendor, renderer, glversion, glslversion);

    // Carregamos os shaders de vértices e de fragmentos que serão utilizados
    // para renderização. Veja slides 180-200 do documento Aula_03_Rendering_Pipeline_Grafico.pdf.
    //
    LoadShadersFromFiles();

    // Carregamos duas imagens para serem utilizadas como textura
    LoadTextureImage("../../data/textures/skybox.jpeg"); // TextureImage0
    LoadTextureImage("../../data/textures/aircraft.jpg"); // TextureImage1
    LoadTextureImage("../../data/textures/moon.jpg"); // TextureImage2
    LoadTextureImage("../../data/textures/asteroid.jpg"); // TextureImage3

    ObjModel aircraft_model("../../data/aircraft.obj");
    ComputeNormals(&aircraft_model);
    BuildTrianglesAndAddToVirtualScene(&aircraft_model);

    ObjModel skybox_model("../../data/sphere.obj");
    ComputeNormals(&skybox_model);
    BuildTrianglesAndAddToVirtualScene(&skybox_model);

    ObjModel asteroid_model("../../data/asteroid.obj");
    ComputeNormals(&asteroid_model);
    BuildTrianglesAndAddToVirtualScene(&asteroid_model);

    if ( argc > 1 )
    {
        ObjModel model(argv[1]);
        BuildTrianglesAndAddToVirtualScene(&model);
    }

    // Inicializamos o código para renderização de texto.
    TextRendering_Init();

    // Habilitamos o Z-buffer. Veja slides 104-116 do documento Aula_09_Projecoes.pdf.
    glEnable(GL_DEPTH_TEST);

    // Habilitamos o Backface Culling. Veja slides 8-13 do documento Aula_02_Fundamentos_Matematicos.pdf, slides 23-34 do documento Aula_13_Clipping_and_Culling.pdf e slides 112-123 do documento Aula_14_Laboratorio_3_Revisao.pdf.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    float tprev = glfwGetTime(); 
    float tnow;
    
    InitEnemies();
    initCheckpoints(); 
    initRandomAsteroids(); 

    bool gouraud = false;

    // Ficamos em um loop infinito, renderizando, até que o usuário feche a janela
    while (!glfwWindowShouldClose(window))
    {
        // Aqui executamos as operações de renderização

        // Definimos a cor do "fundo" do framebuffer como branco.  Tal cor é
        // definida como coeficientes RGBA: Red, Green, Blue, Alpha; isto é:
        // Vermelho, Verde, Azul, Alpha (valor de transparência).
        // Conversaremos sobre sistemas de cores nas aulas de Modelos de Iluminação.
        //
        //           R     G     B     A
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

        // "Pintamos" todos os pixels do framebuffer com a cor definida acima,
        // e também resetamos todos os pixels do Z-buffer (depth buffer).
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Pedimos para a GPU utilizar o programa de GPU criado acima (contendo
        // os shaders de vértice e fragmentos).
        glUseProgram(g_GpuProgramID);

        tnow = glfwGetTime();

        glm::mat4 model = Matrix_Identity(); 
        glm::mat4 aircraft = Matrix_Identity(); 

        glm::mat4 view;
        glm::vec4 camera_position_c;
        glm::vec4 camera_up_vector;
        glm::vec4 up_vec;
        glm::vec4 front_vec;
        glm::vec4 right_vec;
        glm::vec4 camera_lookat_l;
        glm::vec4 camera_view_vector;

        if (!g_UseFirstPersonCamera){
            up_vec = normalizedVec(g_AircraftPosition - moon_position);
            front_vec = normalizedVec(g_AircraftForward - dotproduct(g_AircraftForward, up_vec) * up_vec);
                
            // Atualizamos a global para evitar drift 
            g_AircraftForward = front_vec; 

            right_vec = normalizedVec(crossproduct(up_vec, front_vec));  

            // O ponto para onde a câmera olha (LookAt) é a própria nave
            camera_lookat_l = g_AircraftPosition;

            // Calculamos o deslocamento local baseado no mouse (Theta/Phi) e Distância
            float r = g_CameraDistance;
            float offset_y_local = r * sin(g_CameraPhi);                  // Altura relativa à nave
            float offset_x_local = r * cos(g_CameraPhi) * sin(g_CameraTheta); // Lado relativo
            float offset_z_local = r * cos(g_CameraPhi) * cos(g_CameraTheta); // Distância para trás

            // usando os vetores da nave (Right, Up, Front) como base.
            glm::vec4 final_offset = (right_vec * offset_x_local) + 
                                    (up_vec    * offset_y_local) + 
                                    (front_vec * offset_z_local);

            // A posição da câmera é: Posição da Nave - Deslocamento Calculado
            camera_position_c = camera_lookat_l - final_offset;

            // O vetor "Cima" da câmera agora deve ser a normal da Lua, não o Y global (0,1,0)
            camera_up_vector = up_vec;

            // Gera a matriz View
            camera_view_vector = camera_lookat_l - camera_position_c;
        } else {
            // mesmo processo de cima
            up_vec = normalizedVec(g_AircraftPosition - moon_position);
            front_vec = normalizedVec(g_AircraftForward - dotproduct(g_AircraftForward, up_vec) * up_vec);
                
            g_AircraftForward = front_vec; 

            right_vec = normalizedVec(crossproduct(up_vec, front_vec)); 
            
            // variáveis para primeira pessoa
            float offset_forward = 0.15f; 
            float offset_up      = 0.235f; 
            
            camera_position_c = g_AircraftPosition 
                                + (front_vec * offset_forward) 
                                + (up_vec    * offset_up); 

            // camera esta no bico da nave
            camera_lookat_l = camera_position_c + front_vec;

            camera_up_vector = up_vec;

            camera_view_vector = camera_lookat_l - camera_position_c;           
        }

        // se o jogo nao acabou, está inicializado e nao esta em free flight
        if (!isGameOver() && isIPressed && !g_FreeWorld) 
        {
            moveAircraft(tprev, tnow, g_AircraftPosition);
            moveEnemies(tprev, tnow);
            updateMissiles(tprev, tnow); 
            processCollisions();
        } else if (g_FreeWorld && !isIPressed){ // se o jogo esta em free flight nao pode estar iniciado
            moveAircraft(tprev, tnow, g_AircraftPosition);
        }

        // calculo delta t para deslocamentos
        float delta_t = tnow - tprev;
        g_Asteroid_t += asteroidSpeed * delta_t; 
        
        // curva de bezier
        if (g_Asteroid_t >= 1.0f) {
            // Reinicia o tempo para loopar a curva local
            g_Asteroid_t = 0.0f;
        }

        view = Matrix_Camera_View(camera_position_c, camera_view_vector, camera_up_vector);
            
        // Agora computamos a matriz de Projeção.
        glm::mat4 projection;

        // Note que, no sistema de coordenadas da câmera, os planos near e far
        // estão no sentido negativo! Veja slides 176-204 do documento Aula_09_Projecoes.pdf.
        float nearplane = -0.1f;  // Posição do "near plane"
        float farplane  = -60.0f; // Posição do "far plane"

                if (g_UsePerspectiveProjection)
        {
            // Projeção Perspectiva.
            // Para definição do field of view (FOV), veja slides 205-215 do documento Aula_09_Projecoes.pdf.
            float field_of_view = 3.141592 / 3.0f;
            projection = Matrix_Perspective(field_of_view, g_ScreenRatio, nearplane, farplane);
        }
        else
        {
            // Projeção Ortográfica.
            // Para definição dos valores l, r, b, t ("left", "right", "bottom", "top"),
            // PARA PROJEÇÃO ORTOGRÁFICA veja slides 219-224 do documento Aula_09_Projecoes.pdf.
            // Para simular um "zoom" ortográfico, computamos o valor de "t"
            // utilizando a variável g_CameraDistance.
            float t = 1.5f*g_CameraDistance/2.5f;
            float b = -t;
            float r = t*g_ScreenRatio;
            float l = -r;
            projection = Matrix_Orthographic(l, r, b, t, nearplane, farplane);
        }

        // Enviamos as matrizes "view" e "projection" para a placa de vídeo
        // (GPU). Veja o arquivo "shader_vertex.glsl", onde estas são
        // efetivamente aplicadas em todos os pontos.
        glUniformMatrix4fv(g_view_uniform       , 1 , GL_FALSE , glm::value_ptr(view));
        glUniformMatrix4fv(g_projection_uniform , 1 , GL_FALSE , glm::value_ptr(projection));

        // para decrementar o damage feedback, faz com que ele desapareça
        if (g_DamageTimer > 0.0f) {
            g_DamageTimer -= delta_t;
        }

        // passa se levou um dano para o shader
        bool is_damaged = (g_DamageTimer > 0.0f);
        glUniform1i(g_is_damaged_uniform, is_damaged);

        // Desenha Infinito ao redor da cena
        model = Matrix_Translate(camera_position_c.x,camera_position_c.y,camera_position_c.z) * Matrix_Scale(1.0f/60.0f, 1.0f/60.0f, 1.0f/60.0f);
        glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, SKYBOX);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);   
        DrawVirtualObject("the_sphere");
        glActiveTexture(GL_TEXTURE0); 
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);


        // para alinhar a nave com a lua
        glm::mat4 rotation_align = glm::mat4(1.0f);
        rotation_align[0] = right_vec, 0.0f; // Coluna 0: Eixo X (Right)
        rotation_align[1] = up_vec;    // Coluna 1: Eixo Y (Up)
        rotation_align[2] = front_vec; // Coluna 2: Eixo Z (Front)

        gouraud = false;
        glUniform1i(g_gouraud_uniform, gouraud);

        // Desenhamos o modelo da nave
        aircraft =  Matrix_Translate(g_AircraftPosition.x, g_AircraftPosition.y, g_AircraftPosition.z)
         * rotation_align
         * Matrix_Scale(0.05f, 0.05f, 0.05f)
         * Matrix_Rotate_Y(M_PI_2 * 2);
        glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(aircraft));
        glUniform1i(g_object_id_uniform, AIRCRAFT);

        // desenha todas as peças do objeto aircraft
        for (size_t i = 0; i < aircraft_model.shapes.size(); i++) 
        {
            const char* shapeName = aircraft_model.shapes[i].name.c_str();
            
            DrawVirtualObject(shapeName);
        }

        // Loop para desenhar todos os inimigos
        for (const auto &enemy : g_Enemies) {
            glm::vec4 enemy_pos = enemy.position;
            glm::vec4 up_e = normalizedVec(enemy_pos - moon_position);
            glm::vec4 front_e = enemy.forward;
            glm::vec4 right_e = normalizedVec(crossproduct(up_e, front_e));

            glm::mat4 rotation_align_enemy = glm::mat4(1.0f);
            rotation_align_enemy[0] = right_e;
            rotation_align_enemy[1] = up_e;
            rotation_align_enemy[2] = front_e;

            model = Matrix_Translate(enemy_pos.x, enemy_pos.y, enemy_pos.z)
                    * rotation_align_enemy
                    * Matrix_Scale(0.05f, 0.05f, 0.05f)
                    * Matrix_Rotate_Y(M_PI_2 * 2); 

            glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(g_object_id_uniform, ENEMY); 

            for (size_t i = 0; i < aircraft_model.shapes.size(); i++) {
                DrawVirtualObject(aircraft_model.shapes[i].name.c_str());
            }
        }

        //dedsenha os checkpoints 
        for (const auto &checkpoint_pos : g_Checkpoints)
        {
            glm::mat4 checkpoint_model = Matrix_Translate(checkpoint_pos.x, checkpoint_pos.y, checkpoint_pos.z)
                                    * Matrix_Scale(0.25f/15.0f, 0.25f/15.0f, 0.25f/15.0f); 
                                    
            glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(checkpoint_model));
            glUniform1i(g_object_id_uniform, CHECKPOINT_SPHERE); 
            DrawVirtualObject("the_sphere"); 
        }

        // 1. Calcula a posição na Curva de Bézier.
        glm::vec4 pos = evaluateBezier(g_Asteroid_P0, g_Asteroid_P1, g_Asteroid_P2, g_Asteroid_P3, g_Asteroid_t);

        // desenha o asteroid com am ovimentação de bezier
        model = Matrix_Translate(pos.x, pos.y, pos.z) 
                * Matrix_Scale(asteroidScale, asteroidScale, asteroidScale); 
        
        glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, ASTEROID); 
        DrawVirtualObject("10464_Asteroid_v1"); 

        // ativa gouraud para a lua
        gouraud = true;
        glUniform1i(g_gouraud_uniform, gouraud);

        // Desenhamos o plano do chão (lua)
        model = Matrix_Translate(moon_position.x, moon_position.y, moon_position.z) * Matrix_Scale(15.0f/60.0f, 15.0f/60.0f, 15.0f/60.0f);
        glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, PLANE);
        DrawVirtualObject("the_sphere");

        // desativa gouraud
        gouraud = false;
        glUniform1i(g_gouraud_uniform, gouraud);

        // desenhando o HUD ("progress bar" de vida)
        if (g_AircraftLife > 0 && !isGameOver)
        {
            // Desativamos testes 3D (Z-buffer e Culling) para desenhar o HUD em 2D
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);

            // Matrizes Identidade (espaço NDC, fixo na tela)
            glm::mat4 life_view = Matrix_Identity(); 
            glm::mat4 life_projection = Matrix_Identity(); 
            glm::mat4 life_model = Matrix_Identity();

            glUniformMatrix4fv(g_view_uniform, 1, GL_FALSE, glm::value_ptr(life_view));
            glUniformMatrix4fv(g_projection_uniform, 1, GL_FALSE, glm::value_ptr(life_projection));

            float bar_width_max = 0.001f;     // Largura total em NDC
            float bar_height = 0.0005f;       // Altura em NDC
            float margin_x = 0.1f;         // Margem da borda direita
            float margin_y = 0.1f;         // Margem da borda superior

            float bar_x_center = 1.0f - margin_x - (bar_width_max / 2.0f);
            
            float bar_y_center = 1.0f - margin_y - (bar_height / 2.0f);

            life_model = Matrix_Translate(bar_x_center, bar_y_center, 0.0f) 
                      * Matrix_Scale(bar_width_max, bar_height, 0.0f); 
            
            glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(life_model));
            glUniform1i(g_object_id_uniform, HEALTH_BAR_BACKGROUND);
            DrawVirtualObject("the_sphere"); 

            float current_life_ratio = (float)g_AircraftLife / (float)MAX_LIFE;
            float current_width = bar_width_max * current_life_ratio; 
            
            float x_offset_correction = (current_width - bar_width_max) / 2.0f;
            
            life_model = Matrix_Translate(bar_x_center + x_offset_correction, bar_y_center, 0.0f)
                      * Matrix_Scale(current_width, bar_height, 0.0f);
            
            glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(life_model));
            glUniform1i(g_object_id_uniform, HEALTH_BAR_FOREGROUND);
            DrawVirtualObject("the_sphere"); 

            // Voltamos às configurações 3D
            glEnable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);

            // Restauramos View e Projection para a câmera 3D
            glUniformMatrix4fv(g_view_uniform, 1 , GL_FALSE , glm::value_ptr(view));
            glUniformMatrix4fv(g_projection_uniform , 1 , GL_FALSE , glm::value_ptr(projection));
        }


        // ativa gouraud para os asteroides aleatorios
        // OBS: O asteroid com curva de bezier nao tem gouraud
        gouraud = true;
        glUniform1i(g_gouraud_uniform, gouraud);
        glUniform1i(g_object_id_uniform, ASTEROID); 

        //desenha os asteroides aleatorios
        for (const auto& randomPos : g_RandomAsteroids) {
            model = Matrix_Translate(randomPos.x, randomPos.y, randomPos.z) 
                  * Matrix_Scale(asteroidScale, asteroidScale, asteroidScale); 
            
            glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            DrawVirtualObject("10464_Asteroid_v1");
        }
        
        gouraud = false; 
        glUniform1i(g_gouraud_uniform, gouraud);

        // desenha os misseis
        for (const auto &missile : g_Missiles) {
            if (!missile.isActive) continue;

            glm::vec4 missile_pos = missile.position;
            glm::vec4 up_m = normalizedVec(missile_pos - moon_position);
            glm::vec4 front_m = missile.forward;
            glm::vec4 right_m = normalizedVec(crossproduct(up_m, front_m));
            
            glm::mat4 rotation_align_missile = glm::mat4(1.0f);
            rotation_align_missile[0] = right_m;
            rotation_align_missile[1] = up_m;
            rotation_align_missile[2] = front_m;

            model = Matrix_Translate(missile_pos.x, missile_pos.y, missile_pos.z)
                  * rotation_align_missile
                  * Matrix_Scale(0.1f, 0.1f, 0.1f) 
                  * Matrix_Rotate_Y(M_PI);   

            glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(g_object_id_uniform, AIRCRAFT); // Usa textura da nave

            for (size_t i = 0; i < aircraft_model.shapes.size(); i++) {
                if (aircraft_model.shapes[i].name == "R-40TL") {
                    DrawVirtualObject(aircraft_model.shapes[i].name.c_str());
                    break;
                }
            }
        }

        // guarda a posição passada da nave para o calculo de colisão
        g_AircraftPosition_Prev = g_AircraftPosition; 

        tprev = tnow;

        showText(window);

        // O framebuffer onde OpenGL executa as operações de renderização não
        // é o mesmo que está sendo mostrado para o usuário, caso contrário
        // seria possível ver artefatos conhecidos como "screen tearing". A
        // chamada abaixo faz a troca dos buffers, mostrando para o usuário
        // tudo que foi renderizado pelas funções acima.
        // Veja o link: https://en.wikipedia.org/w/index.php?title=Multiple_buffering&oldid=793452829#Double_buffering_in_computer_graphics
        glfwSwapBuffers(window);

        // Verificamos com o sistema operacional se houve alguma interação do
        // usuário (teclado, mouse, ...). Caso positivo, as funções de callback
        // definidas anteriormente usando glfwSet*Callback() serão chamadas
        // pela biblioteca GLFW.
        glfwPollEvents();
    }

    // Finalizamos o uso dos recursos do sistema operacional
    glfwTerminate();

    // Fim do programa
    return 0;
}

// Função que carrega uma imagem para ser utilizada como textura
void LoadTextureImage(const char* filename)
{
    printf("Carregando imagem \"%s\"... ", filename);

    // Primeiro fazemos a leitura da imagem do disco
    stbi_set_flip_vertically_on_load(true);
    int width;
    int height;
    int channels;
    unsigned char *data = stbi_load(filename, &width, &height, &channels, 3);

    if ( data == NULL )
    {
        fprintf(stderr, "ERROR: Cannot open image file \"%s\".\n", filename);
        std::exit(EXIT_FAILURE);
    }

    printf("OK (%dx%d).\n", width, height);

    // Agora criamos objetos na GPU com OpenGL para armazenar a textura
    GLuint texture_id;
    GLuint sampler_id;
    glGenTextures(1, &texture_id);
    glGenSamplers(1, &sampler_id);

    // Veja slides 95-96 do documento Aula_20_Mapeamento_de_Texturas.pdf
    glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Parâmetros de amostragem da textura.
    glSamplerParameteri(sampler_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glSamplerParameteri(sampler_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Agora enviamos a imagem lida do disco para a GPU
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

    GLuint textureunit = g_NumLoadedTextures;
    glActiveTexture(GL_TEXTURE0 + textureunit);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindSampler(textureunit, sampler_id);

    stbi_image_free(data);

    g_NumLoadedTextures += 1;
}

// Função que desenha um objeto armazenado em g_VirtualScene. Veja definição
// dos objetos na função BuildTrianglesAndAddToVirtualScene().
void DrawVirtualObject(const char* object_name)
{
    // "Ligamos" o VAO. Informamos que queremos utilizar os atributos de
    // vértices apontados pelo VAO criado pela função BuildTrianglesAndAddToVirtualScene(). Veja
    // comentários detalhados dentro da definição de BuildTrianglesAndAddToVirtualScene().
    glBindVertexArray(g_VirtualScene[object_name].vertex_array_object_id);

    // Setamos as variáveis "bbox_min" e "bbox_max" do fragment shader
    // com os parâmetros da axis-aligned bounding box (AABB) do modelo.
    glm::vec3 bbox_min = g_VirtualScene[object_name].bbox_min;
    glm::vec3 bbox_max = g_VirtualScene[object_name].bbox_max;
    glUniform4f(g_bbox_min_uniform, bbox_min.x, bbox_min.y, bbox_min.z, 1.0f);
    glUniform4f(g_bbox_max_uniform, bbox_max.x, bbox_max.y, bbox_max.z, 1.0f);

    // Pedimos para a GPU rasterizar os vértices dos eixos XYZ
    // apontados pelo VAO como linhas. Veja a definição de
    // g_VirtualScene[""] dentro da função BuildTrianglesAndAddToVirtualScene(), e veja
    // a documentação da função glDrawElements() em
    // http://docs.gl/gl3/glDrawElements.
    glDrawElements(
        g_VirtualScene[object_name].rendering_mode,
        g_VirtualScene[object_name].num_indices,
        GL_UNSIGNED_INT,
        (void*)(g_VirtualScene[object_name].first_index * sizeof(GLuint))
    );

    // "Desligamos" o VAO, evitando assim que operações posteriores venham a
    // alterar o mesmo. Isso evita bugs.
    glBindVertexArray(0);
}

// Função que carrega os shaders de vértices e de fragmentos que serão
// utilizados para renderização. Veja slides 180-200 do documento Aula_03_Rendering_Pipeline_Grafico.pdf.
//
void LoadShadersFromFiles()
{
    // Note que o caminho para os arquivos "shader_vertex.glsl" e
    // "shader_fragment.glsl" estão fixados, sendo que assumimos a existência
    // da seguinte estrutura no sistema de arquivos:
    //
    //    + FCG_Lab_01/
    //    |
    //    +--+ bin/
    //    |  |
    //    |  +--+ Release/  (ou Debug/ ou Linux/)
    //    |     |
    //    |     o-- main.exe
    //    |
    //    +--+ src/
    //       |
    //       o-- shader_vertex.glsl
    //       |
    //       o-- shader_fragment.glsl
    //
    GLuint vertex_shader_id = LoadShader_Vertex("../../src/shader_vertex.glsl");
    GLuint fragment_shader_id = LoadShader_Fragment("../../src/shader_fragment.glsl");

    // Deletamos o programa de GPU anterior, caso ele exista.
    if ( g_GpuProgramID != 0 )
        glDeleteProgram(g_GpuProgramID);

    // Criamos um programa de GPU utilizando os shaders carregados acima.
    g_GpuProgramID = CreateGpuProgram(vertex_shader_id, fragment_shader_id);

    // Buscamos o endereço das variáveis definidas dentro do Vertex Shader.
    // Utilizaremos estas variáveis para enviar dados para a placa de vídeo
    // (GPU)! Veja arquivo "shader_vertex.glsl" e "shader_fragment.glsl".
    g_model_uniform      = glGetUniformLocation(g_GpuProgramID, "model"); // Variável da matriz "model"
    g_view_uniform       = glGetUniformLocation(g_GpuProgramID, "view"); // Variável da matriz "view" em shader_vertex.glsl
    g_projection_uniform = glGetUniformLocation(g_GpuProgramID, "projection"); // Variável da matriz "projection" em shader_vertex.glsl
    g_object_id_uniform  = glGetUniformLocation(g_GpuProgramID, "object_id"); // Variável "object_id" em shader_fragment.glsl
    g_bbox_min_uniform   = glGetUniformLocation(g_GpuProgramID, "bbox_min");
    g_bbox_max_uniform   = glGetUniformLocation(g_GpuProgramID, "bbox_max");
    g_is_damaged_uniform = glGetUniformLocation(g_GpuProgramID, "is_damaged"); 

    // Variáveis em "shader_fragment.glsl" para acesso das imagens de textura
    glUseProgram(g_GpuProgramID);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage0"), 0);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage1"), 1);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage2"), 2);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage3"), 3);

    glUseProgram(0);
}

// Função que pega a matriz M e guarda a mesma no topo da pilha
void PushMatrix(glm::mat4 M)
{
    g_MatrixStack.push(M);
}

// Função que remove a matriz atualmente no topo da pilha e armazena a mesma na variável M
void PopMatrix(glm::mat4& M)
{
    if ( g_MatrixStack.empty() )
    {
        M = Matrix_Identity();
    }
    else
    {
        M = g_MatrixStack.top();
        g_MatrixStack.pop();
    }
}

// Função que computa as normais de um ObjModel, caso elas não tenham sido
// especificadas dentro do arquivo ".obj"
void ComputeNormals(ObjModel* model)
{
    if ( !model->attrib.normals.empty() )
        return;

    // Primeiro computamos as normais para todos os TRIÂNGULOS.
    // Segundo, computamos as normais dos VÉRTICES através do método proposto
    // por Gouraud, onde a normal de cada vértice vai ser a média das normais de
    // todas as faces que compartilham este vértice e que pertencem ao mesmo "smoothing group".

    // Obtemos a lista dos smoothing groups que existem no objeto
    std::set<unsigned int> sgroup_ids;
    for (size_t shape = 0; shape < model->shapes.size(); ++shape)
    {
        size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

        assert(model->shapes[shape].mesh.smoothing_group_ids.size() == num_triangles);

        for (size_t triangle = 0; triangle < num_triangles; ++triangle)
        {
            assert(model->shapes[shape].mesh.num_face_vertices[triangle] == 3);
            unsigned int sgroup = model->shapes[shape].mesh.smoothing_group_ids[triangle];
            assert(sgroup >= 0);
            sgroup_ids.insert(sgroup);
        }
    }

    size_t num_vertices = model->attrib.vertices.size() / 3;
    model->attrib.normals.reserve( 3*num_vertices );

    // Processamos um smoothing group por vez
    for (const unsigned int & sgroup : sgroup_ids)
    {
        std::vector<int> num_triangles_per_vertex(num_vertices, 0);
        std::vector<glm::vec4> vertex_normals(num_vertices, glm::vec4(0.0f,0.0f,0.0f,0.0f));

        // Acumulamos as normais dos vértices de todos triângulos deste smoothing group
        for (size_t shape = 0; shape < model->shapes.size(); ++shape)
        {
            size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

            for (size_t triangle = 0; triangle < num_triangles; ++triangle)
            {
                unsigned int sgroup_tri = model->shapes[shape].mesh.smoothing_group_ids[triangle];

                if (sgroup_tri != sgroup)
                    continue;

                glm::vec4  vertices[3];
                for (size_t vertex = 0; vertex < 3; ++vertex)
                {
                    tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                    const float vx = model->attrib.vertices[3*idx.vertex_index + 0];
                    const float vy = model->attrib.vertices[3*idx.vertex_index + 1];
                    const float vz = model->attrib.vertices[3*idx.vertex_index + 2];
                    vertices[vertex] = glm::vec4(vx,vy,vz,1.0);
                }

                const glm::vec4  a = vertices[0];
                const glm::vec4  b = vertices[1];
                const glm::vec4  c = vertices[2];

                const glm::vec4  n = crossproduct(b-a,c-a);

                for (size_t vertex = 0; vertex < 3; ++vertex)
                {
                    tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                    num_triangles_per_vertex[idx.vertex_index] += 1;
                    vertex_normals[idx.vertex_index] += n;
                }
            }
        }

        // Computamos a média das normais acumuladas
        std::vector<size_t> normal_indices(num_vertices, 0);

        for (size_t vertex_index = 0; vertex_index < vertex_normals.size(); ++vertex_index)
        {
            if (num_triangles_per_vertex[vertex_index] == 0)
                continue;

            glm::vec4 n = vertex_normals[vertex_index] / (float)num_triangles_per_vertex[vertex_index];
            n /= norm(n);

            model->attrib.normals.push_back( n.x );
            model->attrib.normals.push_back( n.y );
            model->attrib.normals.push_back( n.z );

            size_t normal_index = (model->attrib.normals.size() / 3) - 1;
            normal_indices[vertex_index] = normal_index;
        }

        // Escrevemos os índices das normais para os vértices dos triângulos deste smoothing group
        for (size_t shape = 0; shape < model->shapes.size(); ++shape)
        {
            size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

            for (size_t triangle = 0; triangle < num_triangles; ++triangle)
            {
                unsigned int sgroup_tri = model->shapes[shape].mesh.smoothing_group_ids[triangle];

                if (sgroup_tri != sgroup)
                    continue;

                for (size_t vertex = 0; vertex < 3; ++vertex)
                {
                    tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                    model->shapes[shape].mesh.indices[3*triangle + vertex].normal_index =
                        normal_indices[ idx.vertex_index ];
                }
            }
        }

    }
}

// Constrói triângulos para futura renderização a partir de um ObjModel.
void BuildTrianglesAndAddToVirtualScene(ObjModel* model)
{
    GLuint vertex_array_object_id;
    glGenVertexArrays(1, &vertex_array_object_id);
    glBindVertexArray(vertex_array_object_id);

    std::vector<GLuint> indices;
    std::vector<float>  model_coefficients;
    std::vector<float>  normal_coefficients;
    std::vector<float>  texture_coefficients;

    for (size_t shape = 0; shape < model->shapes.size(); ++shape)
    {
        size_t first_index = indices.size();
        size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

        const float minval = std::numeric_limits<float>::min();
        const float maxval = std::numeric_limits<float>::max();

        glm::vec3 bbox_min = glm::vec3(maxval,maxval,maxval);
        glm::vec3 bbox_max = glm::vec3(minval,minval,minval);

        for (size_t triangle = 0; triangle < num_triangles; ++triangle)
        {
            assert(model->shapes[shape].mesh.num_face_vertices[triangle] == 3);

            for (size_t vertex = 0; vertex < 3; ++vertex)
            {
                tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];

                indices.push_back(first_index + 3*triangle + vertex);

                const float vx = model->attrib.vertices[3*idx.vertex_index + 0];
                const float vy = model->attrib.vertices[3*idx.vertex_index + 1];
                const float vz = model->attrib.vertices[3*idx.vertex_index + 2];
                //printf("tri %d vert %d = (%.2f, %.2f, %.2f)\n", (int)triangle, (int)vertex, vx, vy, vz);
                model_coefficients.push_back( vx ); // X
                model_coefficients.push_back( vy ); // Y
                model_coefficients.push_back( vz ); // Z
                model_coefficients.push_back( 1.0f ); // W

                bbox_min.x = std::min(bbox_min.x, vx);
                bbox_min.y = std::min(bbox_min.y, vy);
                bbox_min.z = std::min(bbox_min.z, vz);
                bbox_max.x = std::max(bbox_max.x, vx);
                bbox_max.y = std::max(bbox_max.y, vy);
                bbox_max.z = std::max(bbox_max.z, vz);

                // Inspecionando o código da tinyobjloader, o aluno Bernardo
                // Sulzbach (2017/1) apontou que a maneira correta de testar se
                // existem normais e coordenadas de textura no ObjModel é
                // comparando se o índice retornado é -1. Fazemos isso abaixo.

                if ( idx.normal_index != -1 )
                {
                    const float nx = model->attrib.normals[3*idx.normal_index + 0];
                    const float ny = model->attrib.normals[3*idx.normal_index + 1];
                    const float nz = model->attrib.normals[3*idx.normal_index + 2];
                    normal_coefficients.push_back( nx ); // X
                    normal_coefficients.push_back( ny ); // Y
                    normal_coefficients.push_back( nz ); // Z
                    normal_coefficients.push_back( 0.0f ); // W
                }

                if ( idx.texcoord_index != -1 )
                {
                    const float u = model->attrib.texcoords[2*idx.texcoord_index + 0];
                    const float v = model->attrib.texcoords[2*idx.texcoord_index + 1];
                    texture_coefficients.push_back( u );
                    texture_coefficients.push_back( v );
                }
            }
        }

        size_t last_index = indices.size() - 1;

        SceneObject theobject;
        theobject.name           = model->shapes[shape].name;
        theobject.first_index    = first_index; // Primeiro índice
        theobject.num_indices    = last_index - first_index + 1; // Número de indices
        theobject.rendering_mode = GL_TRIANGLES;       // Índices correspondem ao tipo de rasterização GL_TRIANGLES.
        theobject.vertex_array_object_id = vertex_array_object_id;

        theobject.bbox_min = bbox_min;
        theobject.bbox_max = bbox_max;

        g_VirtualScene[model->shapes[shape].name] = theobject;
    }

    GLuint VBO_model_coefficients_id;
    glGenBuffers(1, &VBO_model_coefficients_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_model_coefficients_id);
    glBufferData(GL_ARRAY_BUFFER, model_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, model_coefficients.size() * sizeof(float), model_coefficients.data());
    GLuint location = 0; // "(location = 0)" em "shader_vertex.glsl"
    GLint  number_of_dimensions = 4; // vec4 em "shader_vertex.glsl"
    glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(location);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if ( !normal_coefficients.empty() )
    {
        GLuint VBO_normal_coefficients_id;
        glGenBuffers(1, &VBO_normal_coefficients_id);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_normal_coefficients_id);
        glBufferData(GL_ARRAY_BUFFER, normal_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, normal_coefficients.size() * sizeof(float), normal_coefficients.data());
        location = 1; // "(location = 1)" em "shader_vertex.glsl"
        number_of_dimensions = 4; // vec4 em "shader_vertex.glsl"
        glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(location);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    if ( !texture_coefficients.empty() )
    {
        GLuint VBO_texture_coefficients_id;
        glGenBuffers(1, &VBO_texture_coefficients_id);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_texture_coefficients_id);
        glBufferData(GL_ARRAY_BUFFER, texture_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, texture_coefficients.size() * sizeof(float), texture_coefficients.data());
        location = 2; // "(location = 1)" em "shader_vertex.glsl"
        number_of_dimensions = 2; // vec2 em "shader_vertex.glsl"
        glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(location);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    GLuint indices_id;
    glGenBuffers(1, &indices_id);

    // "Ligamos" o buffer. Note que o tipo agora é GL_ELEMENT_ARRAY_BUFFER.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indices.size() * sizeof(GLuint), indices.data());
    // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); // XXX Errado!
    //

    // "Desligamos" o VAO, evitando assim que operações posteriores venham a
    // alterar o mesmo. Isso evita bugs.
    glBindVertexArray(0);
}

// Carrega um Vertex Shader de um arquivo GLSL. Veja definição de LoadShader() abaixo.
GLuint LoadShader_Vertex(const char* filename)
{
    // Criamos um identificador (ID) para este shader, informando que o mesmo
    // será aplicado nos vértices.
    GLuint vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);

    // Carregamos e compilamos o shader
    LoadShader(filename, vertex_shader_id);

    // Retorna o ID gerado acima
    return vertex_shader_id;
}

// Carrega um Fragment Shader de um arquivo GLSL . Veja definição de LoadShader() abaixo.
GLuint LoadShader_Fragment(const char* filename)
{
    // Criamos um identificador (ID) para este shader, informando que o mesmo
    // será aplicado nos fragmentos.
    GLuint fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);

    // Carregamos e compilamos o shader
    LoadShader(filename, fragment_shader_id);

    // Retorna o ID gerado acima
    return fragment_shader_id;
}

// Função auxilar, utilizada pelas duas funções acima. Carrega código de GPU de
// um arquivo GLSL e faz sua compilação.
void LoadShader(const char* filename, GLuint shader_id)
{
    // Lemos o arquivo de texto indicado pela variável "filename"
    // e colocamos seu conteúdo em memória, apontado pela variável
    // "shader_string".
    std::ifstream file;
    try {
        file.exceptions(std::ifstream::failbit);
        file.open(filename);
    } catch ( std::exception& e ) {
        fprintf(stderr, "ERROR: Cannot open file \"%s\".\n", filename);
        std::exit(EXIT_FAILURE);
    }
    std::stringstream shader;
    shader << file.rdbuf();
    std::string str = shader.str();
    const GLchar* shader_string = str.c_str();
    const GLint   shader_string_length = static_cast<GLint>( str.length() );

    // Define o código do shader GLSL, contido na string "shader_string"
    glShaderSource(shader_id, 1, &shader_string, &shader_string_length);

    // Compila o código do shader GLSL (em tempo de execução)
    glCompileShader(shader_id);

    // Verificamos se ocorreu algum erro ou "warning" durante a compilação
    GLint compiled_ok;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &compiled_ok);

    GLint log_length = 0;
    glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &log_length);

    // Alocamos memória para guardar o log de compilação.
    // A chamada "new" em C++ é equivalente ao "malloc()" do C.
    GLchar* log = new GLchar[log_length];
    glGetShaderInfoLog(shader_id, log_length, &log_length, log);

    // Imprime no terminal qualquer erro ou "warning" de compilação
    if ( log_length != 0 )
    {
        std::string  output;

        if ( !compiled_ok )
        {
            output += "ERROR: OpenGL compilation of \"";
            output += filename;
            output += "\" failed.\n";
            output += "== Start of compilation log\n";
            output += log;
            output += "== End of compilation log\n";
        }
        else
        {
            output += "WARNING: OpenGL compilation of \"";
            output += filename;
            output += "\".\n";
            output += "== Start of compilation log\n";
            output += log;
            output += "== End of compilation log\n";
        }

        fprintf(stderr, "%s", output.c_str());
    }

    // A chamada "delete" em C++ é equivalente ao "free()" do C
    delete [] log;
}

// Esta função cria um programa de GPU, o qual contém obrigatoriamente um
// Vertex Shader e um Fragment Shader.
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id)
{
    // Criamos um identificador (ID) para este programa de GPU
    GLuint program_id = glCreateProgram();

    // Definição dos dois shaders GLSL que devem ser executados pelo programa
    glAttachShader(program_id, vertex_shader_id);
    glAttachShader(program_id, fragment_shader_id);

    // Linkagem dos shaders acima ao programa
    glLinkProgram(program_id);

    // Verificamos se ocorreu algum erro durante a linkagem
    GLint linked_ok = GL_FALSE;
    glGetProgramiv(program_id, GL_LINK_STATUS, &linked_ok);

    // Imprime no terminal qualquer erro de linkagem
    if ( linked_ok == GL_FALSE )
    {
        GLint log_length = 0;
        glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &log_length);

        // Alocamos memória para guardar o log de compilação.
        // A chamada "new" em C++ é equivalente ao "malloc()" do C.
        GLchar* log = new GLchar[log_length];

        glGetProgramInfoLog(program_id, log_length, &log_length, log);

        std::string output;

        output += "ERROR: OpenGL linking of program failed.\n";
        output += "== Start of link log\n";
        output += log;
        output += "\n== End of link log\n";

        // A chamada "delete" em C++ é equivalente ao "free()" do C
        delete [] log;

        fprintf(stderr, "%s", output.c_str());
    }

    // Os "Shader Objects" podem ser marcados para deleção após serem linkados 
    glDeleteShader(vertex_shader_id);
    glDeleteShader(fragment_shader_id);

    // Retornamos o ID gerado acima
    return program_id;
}

// Definição da função que será chamada sempre que a janela do sistema
// operacional for redimensionada, por consequência alterando o tamanho do
// "framebuffer" (região de memória onde são armazenados os pixels da imagem).
void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    // Indicamos que queremos renderizar em toda região do framebuffer. A
    // função "glViewport" define o mapeamento das "normalized device
    // coordinates" (NDC) para "pixel coordinates".  Essa é a operação de
    // "Screen Mapping" ou "Viewport Mapping" vista em aula ({+ViewportMapping2+}).
    glViewport(0, 0, width, height);

    // Atualizamos também a razão que define a proporção da janela (largura /
    // altura), a qual será utilizada na definição das matrizes de projeção,
    // tal que não ocorra distorções durante o processo de "Screen Mapping"
    // acima, quando NDC é mapeado para coordenadas de pixels. Veja slides 205-215 do documento Aula_09_Projecoes.pdf.
    //
    // O cast para float é necessário pois números inteiros são arredondados ao
    // serem divididos!
    g_ScreenRatio = (float)width / height;
}

// Variáveis globais que armazenam a última posição do cursor do mouse, para
// que possamos calcular quanto que o mouse se movimentou entre dois instantes
// de tempo. Utilizadas no callback CursorPosCallback() abaixo.
double g_LastCursorPosX, g_LastCursorPosY;

// Função callback chamada sempre que o usuário aperta algum dos botões do mouse
void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        // Se o usuário pressionou o botão esquerdo do mouse, guardamos a
        // posição atual do cursor nas variáveis g_LastCursorPosX e
        // g_LastCursorPosY.  Também, setamos a variável
        // g_LeftMouseButtonPressed como true, para saber que o usuário está
        // com o botão esquerdo pressionado.
        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_LeftMouseButtonPressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
    {
        // Quando o usuário soltar o botão esquerdo do mouse, atualizamos a
        // variável abaixo para false.
        g_LeftMouseButtonPressed = false;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    {
        // Se o usuário pressionou o botão esquerdo do mouse, guardamos a
        // posição atual do cursor nas variáveis g_LastCursorPosX e
        // g_LastCursorPosY.  Também, setamos a variável
        // g_RightMouseButtonPressed como true, para saber que o usuário está
        // com o botão esquerdo pressionado.
        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_RightMouseButtonPressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
    {
        // Quando o usuário soltar o botão esquerdo do mouse, atualizamos a
        // variável abaixo para false.
        g_RightMouseButtonPressed = false;
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS)
    {
        // Se o usuário pressionou o botão esquerdo do mouse, guardamos a
        // posição atual do cursor nas variáveis g_LastCursorPosX e
        // g_LastCursorPosY.  Também, setamos a variável
        // g_MiddleMouseButtonPressed como true, para saber que o usuário está
        // com o botão esquerdo pressionado.
        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_MiddleMouseButtonPressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE)
    {
        // Quando o usuário soltar o botão esquerdo do mouse, atualizamos a
        // variável abaixo para false.
        g_MiddleMouseButtonPressed = false;
    }
}

// Função callback chamada sempre que o usuário movimentar o cursor do mouse em
// cima da janela OpenGL.
void CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    // Abaixo executamos o seguinte: caso o botão esquerdo do mouse esteja
    // pressionado, computamos quanto que o mouse se movimento desde o último
    // instante de tempo, e usamos esta movimentação para atualizar os
    // parâmetros que definem a posição da câmera dentro da cena virtual.
    // Assim, temos que o usuário consegue controlar a câmera.

    if (g_LeftMouseButtonPressed)
    {
        // Deslocamento do cursor do mouse em x e y de coordenadas de tela!
        float dx = xpos - g_LastCursorPosX;
        float dy = ypos - g_LastCursorPosY;
    
        // Atualizamos parâmetros da câmera com os deslocamentos
        g_CameraTheta -= 0.01f*dx;
        g_CameraPhi   += 0.01f*dy;
    
        const float angle_limit = 3.141592f/2.0f; // 90 graus
        const float epsilon_angle = 0.1f;         // 5 a 6 graus de buffer 
        
        float phimax = angle_limit - epsilon_angle;
        float phimin = -angle_limit + epsilon_angle;
        
        if (g_CameraPhi > phimax)
            g_CameraPhi = phimax;

        if (g_CameraPhi < phimin)
            g_CameraPhi = phimin;
    
        // Atualizamos as variáveis globais para armazenar a posição atual do
        // cursor como sendo a última posição conhecida do cursor.
        g_LastCursorPosX = xpos;
        g_LastCursorPosY = ypos;
    }

    if (g_RightMouseButtonPressed)
    {
        // Deslocamento do cursor do mouse em x e y de coordenadas de tela!
        float dx = xpos - g_LastCursorPosX;
        float dy = ypos - g_LastCursorPosY;
    
        // Atualizamos parâmetros da antebraço com os deslocamentos
        g_ForearmAngleZ -= 0.01f*dx;
        g_ForearmAngleX += 0.01f*dy;
    
        // Atualizamos as variáveis globais para armazenar a posição atual do
        // cursor como sendo a última posição conhecida do cursor.
        g_LastCursorPosX = xpos;
        g_LastCursorPosY = ypos;
    }

    if (g_MiddleMouseButtonPressed)
    {
        // Deslocamento do cursor do mouse em x e y de coordenadas de tela!
        float dx = xpos - g_LastCursorPosX;
        float dy = ypos - g_LastCursorPosY;
    
        // Atualizamos parâmetros da antebraço com os deslocamentos
        g_TorsoPositionX += 0.01f*dx;
        g_TorsoPositionY -= 0.01f*dy;
    
        // Atualizamos as variáveis globais para armazenar a posição atual do
        // cursor como sendo a última posição conhecida do cursor.
        g_LastCursorPosX = xpos;
        g_LastCursorPosY = ypos;
    }
}

// Função callback chamada sempre que o usuário movimenta a "rodinha" do mouse.
void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    // Atualizamos a distância da câmera para a origem utilizando a
    // movimentação da "rodinha", simulando um ZOOM.
    g_CameraDistance -= 0.1f*yoffset;

    // Uma câmera look-at nunca pode estar exatamente "em cima" do ponto para
    // onde ela está olhando, pois isto gera problemas de divisão por zero na
    // definição do sistema de coordenadas da câmera. Isto é, a variável abaixo
    // nunca pode ser zero. Versões anteriores deste código possuíam este bug,
    // o qual foi detectado pelo aluno Vinicius Fraga (2017/2).
    const float verysmallnumber = std::numeric_limits<float>::epsilon();
    if (g_CameraDistance < verysmallnumber)
        g_CameraDistance = verysmallnumber;
}

// Definição da função que será chamada sempre que o usuário pressionar alguma
// tecla do teclado. Veja http://www.glfw.org/docs/latest/input_guide.html#input_key
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mod)
{
    // =======================
    // Não modifique este loop! Ele é utilizando para correção automatizada dos
    // laboratórios. Deve ser sempre o primeiro comando desta função KeyCallback().
    for (int i = 0; i < 10; ++i)
        if (key == GLFW_KEY_0 + i && action == GLFW_PRESS && mod == GLFW_MOD_SHIFT)
            std::exit(100 + i);
    // =======================

    // Se o usuário pressionar a tecla ESC, fechamos a janela.
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

    // O código abaixo implementa a seguinte lógica:
    //   Se apertar tecla X       então g_AngleX += delta;
    //   Se apertar tecla shift+X então g_AngleX -= delta;
    //   Se apertar tecla Y       então g_AngleY += delta;
    //   Se apertar tecla shift+Y então g_AngleY -= delta;
    //   Se apertar tecla Z       então g_AngleZ += delta;
    //   Se apertar tecla shift+Z então g_AngleZ -= delta;

    if (key == GLFW_KEY_R && action == GLFW_PRESS){
        resetGame();
    }

    //Lógica de Movimentação da Aeronave 
    if(key == GLFW_KEY_W)
    {
        if (action == GLFW_PRESS)
            isWPressed = true;

        else if (action == GLFW_RELEASE)
            isWPressed = false;

        else if (action == GLFW_REPEAT)
            ;
    }

    if(key == GLFW_KEY_A)
    {
        if (action == GLFW_PRESS)
            isAPressed = true;

        else if (action == GLFW_RELEASE)
            isAPressed = false;

        else if (action == GLFW_REPEAT)
            ;
    }

    if(key == GLFW_KEY_D)
    {
        if (action == GLFW_PRESS)
            isDPressed = true;

        else if (action == GLFW_RELEASE)
            isDPressed = false;

        else if (action == GLFW_REPEAT)
            ;
    }

    if(key == GLFW_KEY_I)
    {
        if (action == GLFW_PRESS)
            // Usuário apertou a tecla I, então começa o jogo, se apertar novamente pausa
            isIPressed = !isIPressed;
    }

    if (key == GLFW_KEY_C && action == GLFW_PRESS)
    {
        g_UseFirstPersonCamera = !g_UseFirstPersonCamera;
    }
    
    if (key == GLFW_KEY_B && action == GLFW_PRESS)
    {
        g_FreeWorld = !g_FreeWorld;
    }

    // logica de tiro da nave
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
        float tnow = glfwGetTime();
        if (tnow > g_PlayerShotTimer + shootColdownTimer && isIPressed && !g_IsGameOver) {
            
            g_PlayerShotTimer = tnow; // Resetar o temporizador
            
            glm::vec4 up_vec = normalizedVec(g_AircraftPosition - moon_position);
            glm::vec4 front_vec = g_AircraftForward; 
            glm::vec4 right_vec = normalizedVec(crossproduct(up_vec, front_vec)); 
            
            glm::vec4 missile_start_pos = g_AircraftPosition + front_vec * 0.5f + right_vec * -0.45f; 

            fireMissile(missile_start_pos, front_vec, 0); // ownerId 0 = Nave
        }
    }

    if (key == GLFW_KEY_R && action == GLFW_PRESS)
    {
        LoadShadersFromFiles();
        fprintf(stdout,"Shaders recarregados!\n");
        fflush(stdout);
    }
}

// Definimos o callback para impressão de erros da GLFW no terminal
void ErrorCallback(int error, const char* description)
{
    fprintf(stderr, "ERROR: GLFW: %s\n", description);
}

// Esta função recebe um vértice com coordenadas de modelo p_model e passa o
// mesmo por todos os sistemas de coordenadas armazenados nas matrizes model,
// view, e projection; e escreve na tela as matrizes e pontos resultantes
// dessas transformações.
void TextRendering_ShowModelViewProjection(
    GLFWwindow* window,
    glm::mat4 projection,
    glm::mat4 view,
    glm::mat4 model,
    glm::vec4 p_model
)
{
    if ( !g_ShowInfoText )
        return;

    glm::vec4 p_world = model*p_model;
    glm::vec4 p_camera = view*p_world;
    glm::vec4 p_clip = projection*p_camera;
    glm::vec4 p_ndc = p_clip / p_clip.w;

    float pad = TextRendering_LineHeight(window);

    TextRendering_PrintString(window, " Model matrix             Model     In World Coords.", -1.0f, 1.0f-pad, 1.0f);
    TextRendering_PrintMatrixVectorProduct(window, model, p_model, -1.0f, 1.0f-2*pad, 1.0f);

    TextRendering_PrintString(window, "                                        |  ", -1.0f, 1.0f-6*pad, 1.0f);
    TextRendering_PrintString(window, "                            .-----------'  ", -1.0f, 1.0f-7*pad, 1.0f);
    TextRendering_PrintString(window, "                            V              ", -1.0f, 1.0f-8*pad, 1.0f);

    TextRendering_PrintString(window, " View matrix              World     In Camera Coords.", -1.0f, 1.0f-9*pad, 1.0f);
    TextRendering_PrintMatrixVectorProduct(window, view, p_world, -1.0f, 1.0f-10*pad, 1.0f);

    TextRendering_PrintString(window, "                                        |  ", -1.0f, 1.0f-14*pad, 1.0f);
    TextRendering_PrintString(window, "                            .-----------'  ", -1.0f, 1.0f-15*pad, 1.0f);
    TextRendering_PrintString(window, "                            V              ", -1.0f, 1.0f-16*pad, 1.0f);

    TextRendering_PrintString(window, " Projection matrix        Camera                    In NDC", -1.0f, 1.0f-17*pad, 1.0f);
    TextRendering_PrintMatrixVectorProductDivW(window, projection, p_camera, -1.0f, 1.0f-18*pad, 1.0f);

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    glm::vec2 a = glm::vec2(-1, -1);
    glm::vec2 b = glm::vec2(+1, +1);
    glm::vec2 p = glm::vec2( 0,  0);
    glm::vec2 q = glm::vec2(width, height);

    glm::mat4 viewport_mapping = Matrix(
        (q.x - p.x)/(b.x-a.x), 0.0f, 0.0f, (b.x*p.x - a.x*q.x)/(b.x-a.x),
        0.0f, (q.y - p.y)/(b.y-a.y), 0.0f, (b.y*p.y - a.y*q.y)/(b.y-a.y),
        0.0f , 0.0f , 1.0f , 0.0f ,
        0.0f , 0.0f , 0.0f , 1.0f
    );

    TextRendering_PrintString(window, "                                                       |  ", -1.0f, 1.0f-22*pad, 1.0f);
    TextRendering_PrintString(window, "                            .--------------------------'  ", -1.0f, 1.0f-23*pad, 1.0f);
    TextRendering_PrintString(window, "                            V                           ", -1.0f, 1.0f-24*pad, 1.0f);

    TextRendering_PrintString(window, " Viewport matrix           NDC      In Pixel Coords.", -1.0f, 1.0f-25*pad, 1.0f);
    TextRendering_PrintMatrixVectorProductMoreDigits(window, viewport_mapping, p_ndc, -1.0f, 1.0f-26*pad, 1.0f);
}

// Escrevemos na tela os ângulos de Euler definidos nas variáveis globais
// g_AngleX, g_AngleY, e g_AngleZ.
void TextRendering_ShowEulerAngles(GLFWwindow* window)
{
    if ( !g_ShowInfoText )
        return;

    float pad = TextRendering_LineHeight(window);

    char buffer[80];
    snprintf(buffer, 80, "Euler Angles rotation matrix = Z(%.2f)*Y(%.2f)*X(%.2f)\n", g_AngleZ, g_AngleY, g_AngleX);

    TextRendering_PrintString(window, buffer, -1.0f+pad/10, -1.0f+2*pad/10, 1.0f);
}

// Escrevemos na tela qual matriz de projeção está sendo utilizada.
void TextRendering_ShowProjection(GLFWwindow* window)
{
    if ( !g_ShowInfoText )
        return;

    float lineheight = TextRendering_LineHeight(window);
    float charwidth = TextRendering_CharWidth(window);

    if ( g_UsePerspectiveProjection )
        TextRendering_PrintString(window, "Perspective", 1.0f-13*charwidth, -1.0f+2*lineheight/10, 1.0f);
    else
        TextRendering_PrintString(window, "Orthographic", 1.0f-13*charwidth, -1.0f+2*lineheight/10, 1.0f);
}

// Escrevemos na tela o número de quadros renderizados por segundo (frames per
// second).
void TextRendering_ShowFramesPerSecond(GLFWwindow* window)
{
    if ( !g_ShowInfoText )
        return;

    // Variáveis estáticas (static) mantém seus valores entre chamadas
    // subsequentes da função!
    static float old_seconds = (float)glfwGetTime();
    static int   ellapsed_frames = 0;
    static char  buffer[20] = "?? fps";
    static int   numchars = 7;

    ellapsed_frames += 1;

    // Recuperamos o número de segundos que passou desde a execução do programa
    float seconds = (float)glfwGetTime();

    // Número de segundos desde o último cálculo do fps
    float ellapsed_seconds = seconds - old_seconds;

    if ( ellapsed_seconds > 1.0f )
    {
     
        old_seconds = seconds;
        ellapsed_frames = 0;
    }

    float lineheight = TextRendering_LineHeight(window);
    float charwidth = TextRendering_CharWidth(window);

    TextRendering_PrintString(window, buffer, 1.0f-(numchars + 1)*charwidth, 1.0f-lineheight, 1.0f);
}

// Função para debugging: imprime no terminal todas informações de um modelo
// geométrico carregado de um arquivo ".obj".
// Veja: https://github.com/syoyo/tinyobjloader/blob/22883def8db9ef1f3ffb9b404318e7dd25fdbb51/loader_example.cc#L98
void PrintObjModelInfo(ObjModel* model)
{
  const tinyobj::attrib_t                & attrib    = model->attrib;
  const std::vector<tinyobj::shape_t>    & shapes    = model->shapes;
  const std::vector<tinyobj::material_t> & materials = model->materials;

  printf("# of vertices  : %d\n", (int)(attrib.vertices.size() / 3));
  printf("# of normals   : %d\n", (int)(attrib.normals.size() / 3));
  printf("# of texcoords : %d\n", (int)(attrib.texcoords.size() / 2));
  printf("# of shapes    : %d\n", (int)shapes.size());
  printf("# of materials : %d\n", (int)materials.size());

  for (size_t v = 0; v < attrib.vertices.size() / 3; v++) {
    printf("  v[%ld] = (%f, %f, %f)\n", static_cast<long>(v),
           static_cast<const double>(attrib.vertices[3 * v + 0]),
           static_cast<const double>(attrib.vertices[3 * v + 1]),
           static_cast<const double>(attrib.vertices[3 * v + 2]));
  }

  for (size_t v = 0; v < attrib.normals.size() / 3; v++) {
    printf("  n[%ld] = (%f, %f, %f)\n", static_cast<long>(v),
           static_cast<const double>(attrib.normals[3 * v + 0]),
           static_cast<const double>(attrib.normals[3 * v + 1]),
           static_cast<const double>(attrib.normals[3 * v + 2]));
  }

  for (size_t v = 0; v < attrib.texcoords.size() / 2; v++) {
    printf("  uv[%ld] = (%f, %f)\n", static_cast<long>(v),
           static_cast<const double>(attrib.texcoords[2 * v + 0]),
           static_cast<const double>(attrib.texcoords[2 * v + 1]));
  }

  // For each shape
  for (size_t i = 0; i < shapes.size(); i++) {
    printf("shape[%ld].name = %s\n", static_cast<long>(i),
           shapes[i].name.c_str());
    printf("Size of shape[%ld].indices: %lu\n", static_cast<long>(i),
           static_cast<unsigned long>(shapes[i].mesh.indices.size()));

    size_t index_offset = 0;

    assert(shapes[i].mesh.num_face_vertices.size() ==
           shapes[i].mesh.material_ids.size());

    printf("shape[%ld].num_faces: %lu\n", static_cast<long>(i),
           static_cast<unsigned long>(shapes[i].mesh.num_face_vertices.size()));

    // For each face
    for (size_t f = 0; f < shapes[i].mesh.num_face_vertices.size(); f++) {
      size_t fnum = shapes[i].mesh.num_face_vertices[f];

      printf("  face[%ld].fnum = %ld\n", static_cast<long>(f),
             static_cast<unsigned long>(fnum));

      // For each vertex in the face
      for (size_t v = 0; v < fnum; v++) {
        tinyobj::index_t idx = shapes[i].mesh.indices[index_offset + v];
        printf("    face[%ld].v[%ld].idx = %d/%d/%d\n", static_cast<long>(f),
               static_cast<long>(v), idx.vertex_index, idx.normal_index,
               idx.texcoord_index);
      }

      printf("  face[%ld].material_id = %d\n", static_cast<long>(f),
             shapes[i].mesh.material_ids[f]);

      index_offset += fnum;
    }

    printf("shape[%ld].num_tags: %lu\n", static_cast<long>(i),
           static_cast<unsigned long>(shapes[i].mesh.tags.size()));
    for (size_t t = 0; t < shapes[i].mesh.tags.size(); t++) {
      printf("  tag[%ld] = %s ", static_cast<long>(t),
             shapes[i].mesh.tags[t].name.c_str());
      printf(" ints: [");
      for (size_t j = 0; j < shapes[i].mesh.tags[t].intValues.size(); ++j) {
        printf("%ld", static_cast<long>(shapes[i].mesh.tags[t].intValues[j]));
        if (j < (shapes[i].mesh.tags[t].intValues.size() - 1)) {
          printf(", ");
        }
      }
      printf("]");

      printf(" floats: [");
      for (size_t j = 0; j < shapes[i].mesh.tags[t].floatValues.size(); ++j) {
        printf("%f", static_cast<const double>(
                         shapes[i].mesh.tags[t].floatValues[j]));
        if (j < (shapes[i].mesh.tags[t].floatValues.size() - 1)) {
          printf(", ");
        }
      }
      printf("]");

      printf(" strings: [");
      for (size_t j = 0; j < shapes[i].mesh.tags[t].stringValues.size(); ++j) {
        printf("%s", shapes[i].mesh.tags[t].stringValues[j].c_str());
        if (j < (shapes[i].mesh.tags[t].stringValues.size() - 1)) {
          printf(", ");
        }
      }
      printf("]");
      printf("\n");
    }
  }

  for (size_t i = 0; i < materials.size(); i++) {
    printf("material[%ld].name = %s\n", static_cast<long>(i),
           materials[i].name.c_str());
    printf("  material.Ka = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].ambient[0]),
           static_cast<const double>(materials[i].ambient[1]),
           static_cast<const double>(materials[i].ambient[2]));
    printf("  material.Kd = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].diffuse[0]),
           static_cast<const double>(materials[i].diffuse[1]),
           static_cast<const double>(materials[i].diffuse[2]));
    printf("  material.Ks = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].specular[0]),
           static_cast<const double>(materials[i].specular[1]),
           static_cast<const double>(materials[i].specular[2]));
    printf("  material.Tr = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].transmittance[0]),
           static_cast<const double>(materials[i].transmittance[1]),
           static_cast<const double>(materials[i].transmittance[2]));
    printf("  material.Ke = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].emission[0]),
           static_cast<const double>(materials[i].emission[1]),
           static_cast<const double>(materials[i].emission[2]));
    printf("  material.Ns = %f\n",
           static_cast<const double>(materials[i].shininess));
    printf("  material.Ni = %f\n", static_cast<const double>(materials[i].ior));
    printf("  material.dissolve = %f\n",
           static_cast<const double>(materials[i].dissolve));
    printf("  material.illum = %d\n", materials[i].illum);
    printf("  material.map_Ka = %s\n", materials[i].ambient_texname.c_str());
    printf("  material.map_Kd = %s\n", materials[i].diffuse_texname.c_str());
    printf("  material.map_Ks = %s\n", materials[i].specular_texname.c_str());
    printf("  material.map_Ns = %s\n",
           materials[i].specular_highlight_texname.c_str());
    printf("  material.map_bump = %s\n", materials[i].bump_texname.c_str());
    printf("  material.map_d = %s\n", materials[i].alpha_texname.c_str());
    printf("  material.disp = %s\n", materials[i].displacement_texname.c_str());
    printf("  <<PBR>>\n");
    printf("  material.Pr     = %f\n", materials[i].roughness);
    printf("  material.Pm     = %f\n", materials[i].metallic);
    printf("  material.Ps     = %f\n", materials[i].sheen);
    printf("  material.Pc     = %f\n", materials[i].clearcoat_thickness);
    printf("  material.Pcr    = %f\n", materials[i].clearcoat_thickness);
    printf("  material.aniso  = %f\n", materials[i].anisotropy);
    printf("  material.anisor = %f\n", materials[i].anisotropy_rotation);
    printf("  material.map_Ke = %s\n", materials[i].emissive_texname.c_str());
    printf("  material.map_Pr = %s\n", materials[i].roughness_texname.c_str());
    printf("  material.map_Pm = %s\n", materials[i].metallic_texname.c_str());
    printf("  material.map_Ps = %s\n", materials[i].sheen_texname.c_str());
    printf("  material.norm   = %s\n", materials[i].normal_texname.c_str());
    std::map<std::string, std::string>::const_iterator it(
        materials[i].unknown_parameter.begin());
    std::map<std::string, std::string>::const_iterator itEnd(
        materials[i].unknown_parameter.end());

    for (; it != itEnd; it++) {
      printf("  material.%s = %s\n", it->first.c_str(), it->second.c_str());
    }
    printf("\n");
  }
}

void moveAircraft(float tprev, float tnow, glm::vec4& aircraft_position) {
    float delta_t = tnow - tprev;
    float speed = 8.0f;         
    float turn_speed = 3.0f;   
    float fixedDistance = 16.0f; 
    // calcula os vetores de posição da nave
    glm::vec4 currentPos = aircraft_position;

    glm::vec4 up_vec = normalizedVec(currentPos - moon_position);

    g_AircraftForward = normalizedVec(g_AircraftForward - dotproduct(g_AircraftForward, up_vec) * up_vec);

    glm::vec4 right_vec = normalizedVec(crossproduct(up_vec, g_AircraftForward));

    // logica de movimentação, baseado na distancia da lua, centro (posição da lua) e alinhando o obj da nave com a tangente da lua para sempre estar alinhado
    if (isWPressed) {
        float angle = speed * delta_t * 0.05f; 


        glm::mat4 rotMatrix = Matrix_Rotate(angle, right_vec);

        glm::vec4 relativePosition = currentPos - moon_position;
        relativePosition = rotMatrix * relativePosition;
        currentPos = moon_position + relativePosition;

        glm::vec4 fwd = g_AircraftForward;
        g_AircraftForward = normalizedVec(rotMatrix * fwd);

        
        // permite direita e esquerda, mas com velocidade reduzida
        if (isAPressed || isDPressed) {
            float angle = turn_speed * delta_t;
            if (isDPressed) angle = -angle; // Inverte para Direita

            // Rotação em torno do eixo UP (Normal da lua)
            glm::mat4 rotMatrix = Matrix_Rotate(angle, up_vec);
            
            glm::vec4 fwd = g_AircraftForward;
            g_AircraftForward = normalizedVec(rotMatrix * fwd);
        }
    }

    aircraft_position = currentPos;
    
    glm::vec4 finalDir = normalizedVec(aircraft_position - moon_position);
    aircraft_position = moon_position + (finalDir * fixedDistance);
}

// Função auxiliar para gerar float aleatório entre min e max
float randomFloat(float min, float max) {
    return min + static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / (max - min)));
}

void InitEnemies() {    
    for(int i = 0; i < 3; i++) {
        Enemy e;
        
        // Posição aleatória na esfera 
        glm::vec4 randomPos = glm::vec4(randomFloat(-1.0f, 1.0f), randomFloat(-1.0f, 1.0f), randomFloat(-1.0f, 1.0f), 0.0f);
        randomPos = normalizedVec(randomPos) * 16.0f; 
        
        e.position = randomPos + moon_position;
        
        glm::vec4 up = normalizedVec(e.position - moon_position);
        glm::vec4 randomDir = glm::vec4(randomFloat(-1.0f, 1.0f), randomFloat(-1.0f, 1.0f), randomFloat(-1.0f, 1.0f), 0.0f);
        
        e.forward = normalizedVec(randomDir - dotproduct(randomDir, up) * up);
        
        e.speed = 5.0f;
        e.changeDirTimer = randomFloat(1.0f, 3.0f); 
        
        g_Enemies.push_back(e);
    }
}

void moveEnemies(float tprev, float tnow) {
    float delta_t = tnow - tprev;
    float fixedDistance = 16.0f; 


    // mesma ideia do move aircraft, sempre levando em conta a posição da lua, a distancia do centro e tangente com a lua
    // o inimigo sempre olha e busca chegar na posição do jogador, como uma caça
    for (auto &enemy : g_Enemies) {
        glm::vec4 currentPos = enemy.position;
        glm::vec4 up_vec = normalizedVec(currentPos - moon_position);
        
        // Garante que o forward é tangente à superfície
        glm::vec4 projection_fwd = dotproduct(enemy.forward, up_vec) * up_vec;
        enemy.forward = enemy.forward - projection_fwd;
        enemy.forward = normalizedVec(enemy.forward); 
        
        // Vetor Desejado (Inimigo -> Jogador)
        glm::vec4 vector_to_target = g_AircraftPosition - enemy.position;
        vector_to_target.w = 0.0f; 
        
        // Projeta o vetor Desejado no plano tangente (impede que ele tente perfurar a Lua)
        projection_fwd = dotproduct(vector_to_target, up_vec) * up_vec;
        glm::vec4 desired_forward = vector_to_target - projection_fwd;
        desired_forward = normalizedVec(desired_forward); // Vetor para onde o inimigo deve olhar

        // Calcula o eixo e o ângulo para girar de enemy.forward para desired_forward
        glm::vec4 rotation_axis = crossproduct(enemy.forward, desired_forward);
        
        float dot_value = dotproduct(enemy.forward, desired_forward);
        
        // Limita o valor do dotproduct para evitar erros de floating point com acos
        if (dot_value > 1.0f) dot_value = 1.0f;
        if (dot_value < -1.0f) dot_value = -1.0f;

        float angle_to_turn = std::acos(dot_value);
        
        // Limita a rotação para a taxa máxima de giro (para um movimento suave)
        if (angle_to_turn > turnRate * delta_t) {
            angle_to_turn = turnRate * delta_t;
        }

        // Aplica a rotação de Steering
        glm::mat4 turnMatrix = Matrix_Rotate(angle_to_turn, rotation_axis);
        enemy.forward = normalizedVec(turnMatrix * enemy.forward);

        
        glm::vec4 right_vec = normalizedVec(crossproduct(up_vec, enemy.forward));
        
        float angle_of_advance = enemySpeed * delta_t * 0.05f; 
        glm::mat4 moveMatrix = Matrix_Rotate(angle_of_advance, right_vec); 

        glm::vec4 relativePosition = currentPos - moon_position;
        relativePosition = moveMatrix * relativePosition;
        
        // Reprojetar na distância fixa de órbita
        glm::vec4 finalDir = relativePosition; finalDir.w = 0.0f;
        finalDir = normalizedVec(finalDir);
        enemy.position = moon_position + (finalDir * fixedDistance);
        enemy.position.w = 1.0f; 

        // Timer para o disparo do inimigo
        static float enemy_shot_timer = 0.0f;
        enemy_shot_timer += delta_t; 

        for (auto &enemy : g_Enemies) {
            // Lógica de Disparo do Inimigo
            if (enemy_shot_timer > enemyShotSpeed) {
                
                glm::vec4 enemy_forward = enemy.forward;
                glm::vec4 missile_start_pos = enemy.position + enemy_forward * 0.5f; 
                
                fireMissile(missile_start_pos, enemy_forward, 1); // ownerId 1 
            }
        }
        
        if (enemy_shot_timer > enemyShotSpeed) {
            enemy_shot_timer = 0.0f; // Resetar timer após o loop
        }
    }
}


// Fórmula da Curva de Bézier Cúbica 
glm::vec4 evaluateBezier(glm::vec4 p0, glm::vec4 p1, glm::vec4 p2, glm::vec4 p3, float t) {
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;

    return (uuu * p0) + (3 * uu * t * p1) + (3 * u * tt * p2) + (ttt * p3);
}

// Função que testa as colisões
void processCollisions() {
    // =======================================================
    // Colisão Nave vs. Inimigo (ESFERA-ESFERA)
    // =======================================================
    
    // Cria a Bounding Sphere da Nave
    BoundingSphere aircraftSphere = getAircraftBoundingSphere(g_AircraftPosition);
    
    // Iteramos sobre os inimigos
    for (const auto &enemy : g_Enemies) {
        
        // Cria a Bounding Sphere do Inimigo
        BoundingSphere enemySphere = getEnemyBoundingSphere(enemy.position);

        for (int i = g_Enemies.size() - 1; i >= 0; --i) {
            Enemy &enemy = g_Enemies[i];
            
            BoundingSphere enemySphere = getEnemyBoundingSphere(enemy.position);

            if (checkSphereSphereCollision(aircraftSphere, enemySphere)) {                
                // Aplica dano (se o jogo ainda não acabou)
                if (!g_IsGameOver) {
                    g_AircraftLife -= 1;
                
                    // Verifica Game Over
                    if (isGameOver()) {
                    }

                    g_DamageTimer = damageDuration;
                }

                // Destrói o inimigo 
                std::swap(g_Enemies[i], g_Enemies.back());
                g_Enemies.pop_back(); 
            }
        }
    }
    
    // =======================================================
    // Coletar Checkpoints (RAIO-ESFERA)
    // =======================================================
    
    // A nave precisa ter se movido para que o raio de trajetória seja válido
    if (g_AircraftPosition_Prev != g_AircraftPosition) {
        
        // 1. Define o Raio (Linha de Movimento da Nave)
        Ray trajectoryRay;
        trajectoryRay.origin = g_AircraftPosition_Prev; // Posição anterior
        
        glm::vec4 movement_vector = g_AircraftPosition - g_AircraftPosition_Prev;
        movement_vector.w = 0.0f; 
        
        trajectoryRay.direction = normalizedVec(movement_vector);
        float movement_distance = norm(movement_vector); 

        for (int i = g_Checkpoints.size() - 1; i >= 0; --i) {
            glm::vec4 checkpoint_pos = g_Checkpoints[i];
            
            BoundingSphere checkpointSphere = getCheckpointBoundingSphere(checkpoint_pos); 
            
            float t_hit; // Distância do ponto de colisão

            if (checkRaySphereCollision(trajectoryRay, checkpointSphere, t_hit)) {
                
                // Verifica se o ponto de intersecção (t_hit) ocorreu *dentro* do segmento de movimento
                if (t_hit >= 0.0f && t_hit <= movement_distance) {                     
                    // Remove o checkpoint coletado 
                    std::swap(g_Checkpoints[i], g_Checkpoints.back());
                    g_Checkpoints.pop_back(); 
                    
                    // Recuperar vida ao pegar um checkpoint
                    if (g_AircraftLife < MAX_LIFE) {
                         g_AircraftLife++; 
                    }
                }
            }
        }
    }
    
    
    bool collisionOccurred = false;

    // =======================================================
    // Colisão Nave vs. TODOS Asteroides (CILINDRO-ESFERA)
    // =======================================================
    
    // Itera de trás para frente para permitir a remoção segura
    for (int i = g_RandomAsteroids.size() - 1; i >= 0; --i) 
    {
        glm::vec4 asteroidPos = g_RandomAsteroids[i];
        BoundingCylinder asteroidCylinder = getAsteroidBoundingCylinder(asteroidPos);
        
        if (checkCylinderSphereCollision(asteroidCylinder, aircraftSphere)) { 
            if (!g_IsGameOver && !collisionOccurred) {
                g_AircraftLife -= 1; 
                collisionOccurred = true;

                g_DamageTimer = damageDuration;
            }
            std::swap(g_RandomAsteroids[i], g_RandomAsteroids.back());
            g_RandomAsteroids.pop_back(); 
        }
    }

    // Itera de trás para frente para permitir a remoção segura dos mísseis
    for (int i = g_Missiles.size() - 1; i >= 0; --i) {
        Missile &missile = g_Missiles[i];
        
        if (!missile.isActive) continue;

        BoundingSphere missileSphere = {missile.position, missileRadius};
        
        bool hit = false;
        
        // =======================================================
        // Colisão Nave/Inimigo (Missil vs. Nave/Inimigo)
        // =======================================================

        if (missile.ownerId == 0) { // Míssil da Nave -> Colide com Inimigos
            for (int j = g_Enemies.size() - 1; j >= 0; --j) {
                Enemy &enemy = g_Enemies[j];
                BoundingSphere enemySphere = getEnemyBoundingSphere(enemy.position);

                if (checkSphereSphereCollision(missileSphere, enemySphere)) {                    
                    // Destrói o inimigo
                    std::swap(g_Enemies[j], g_Enemies.back());
                    g_Enemies.pop_back();
                    hit = true;
                    break; 
                }
            }
        } else { // Míssil do Inimigo -> Colide com a Nave
            BoundingSphere aircraftSphere = getAircraftBoundingSphere(g_AircraftPosition);
            if (checkSphereSphereCollision(missileSphere, aircraftSphere)) {                
                // Aplica dano (se o jogo ainda não acabou)
                if (!g_IsGameOver) {
                    g_AircraftLife -= 1;

                    g_DamageTimer = damageDuration; 
                }
                hit = true;
            }
        }
        
        // =======================================================
        // Colisão Missil vs. Asteroides Aleatórios (Esfera vs. Cilindro)
        // =======================================================

        // Itera sobre os asteroides aleatórios (g_RandomAsteroids)
        for (int j = g_RandomAsteroids.size() - 1; j >= 0; --j) 
        {
            glm::vec4 asteroidPos = g_RandomAsteroids[j];
            BoundingCylinder asteroidCylinder = getAsteroidBoundingCylinder(asteroidPos);
            
            if (checkCylinderSphereCollision(asteroidCylinder, missileSphere)) {                 
                std::swap(g_RandomAsteroids[j], g_RandomAsteroids.back());
                g_RandomAsteroids.pop_back(); 
                
                hit = true;
                break; 
            }
        }

        if (hit) {
            missile.isActive = false;
            std::swap(g_Missiles[i], g_Missiles.back());
            g_Missiles.pop_back();
        }
    }
}

glm::vec4 normalizedVec(glm::vec4 v) {
    float n = norm(v);
    v = v / n;
    v.w = 0.0f; 
    
    return v;
}

// função que testa se o jogo acabou
bool isGameOver() {
    if (g_AircraftLife <= 0) {
        g_IsGameOver = true;
        return true; 
    }
    
    if (g_Checkpoints.empty())
    {
        g_IsGameOver = true;
        return true; 
    }

    return false;
}

// reseta o jogo
void resetGame() {
    g_AircraftLife = 3;
    g_IsGameOver = false;
    g_Enemies.clear();
    g_AircraftPosition = glm::vec4(0.0f, 0.0f, 16.0f, 1.0f);
    g_AircraftPosition_Prev = g_AircraftPosition;
    g_AircraftForward = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
    InitEnemies();
    initCheckpoints();
    initRandomAsteroids();
    g_DamageTimer = 0.0f;    
    g_Asteroid_t = 0.0f; 
    glm::vec4 g_Asteroid_P0 = glm::vec4(16.0f, 0.0f, 0.0f, 1.0f);    
    glm::vec4 g_Asteroid_P1 = glm::vec4(12.0f, 4.0f, 4.0f, 1.0f);    
    glm::vec4 g_Asteroid_P2 = glm::vec4(20.0f, 4.0f, -4.0f, 1.0f);   
    glm::vec4 g_Asteroid_P3 = glm::vec4(16.0f, 0.0f, 0.0f, 1.0f);  
    g_Missiles.clear();
}


// função exlusiva para adicionar as orientações e feedbacks na tela
void showText(GLFWwindow* window){
    float pad = TextRendering_LineHeight(window);
    char buffer[80];
    
    float margin_x = 0.05f;
    float margin_y_top = 0.05f;
    float margin_y_bottom = 0.95f; 

    float est_width = 15.0f * TextRendering_CharWidth(window);
    
    if (g_AircraftLife <= 0) {
        float current_y = -1.0f + margin_y_top; 

        TextRendering_PrintString(window, "GAME OVER! (Pressione ESC para sair)", -1.0f + margin_x, current_y, 1.0f);
        current_y += pad; 
        
        TextRendering_PrintString(window, "Pressione R para reiniciar", -1.0f + margin_x, current_y, 1.0f);
        current_y += pad;
    } else if (g_Checkpoints.empty()) {
        float current_y = -1.0f + margin_y_top; 

        TextRendering_PrintString(window, "PARABENS! VOCE VENCEU! (Pressione ESC para sair)", -1.0f + margin_x, current_y, 1.0f);
        current_y += pad; 
        
        TextRendering_PrintString(window, "Pressione R para reiniciar", -1.0f + margin_x, current_y, 1.0f);
        current_y += pad;
    } else if (!g_IsGameOver) {
        snprintf(buffer, 80, "Vida: %d", g_AircraftLife);
        
        TextRendering_PrintString(window, buffer, 1.0f - margin_x - est_width, 1.0f - margin_y_top, 1.0f);
        float current_y = -1.0f + margin_y_top;

        TextRendering_PrintString(window, "Pressione ESC para sair", -1.0f + margin_x, current_y, 1.0f);
        current_y += pad; 
        
        TextRendering_PrintString(window, "C para alternar câmera livre", -1.0f + margin_x, current_y, 1.0f);
        current_y += pad;

        TextRendering_PrintString(window, "Space para atirar", -1.0f + margin_x, current_y, 1.0f);
        current_y += pad;

        TextRendering_PrintString(window, "Use W,A,D para mover a aeronave", -1.0f + margin_x, current_y, 1.0f);
        current_y += pad;

        TextRendering_PrintString(window, "Use o mouse para olhar ao redor (camera livre)", -1.0f + margin_x, current_y, 1.0f);
        current_y += pad;

        TextRendering_PrintString(window, "Pressione B para voo livre (desmonstração)", -1.0f + margin_x, current_y, 1.0f);
        current_y += pad;
        
        TextRendering_PrintString(window, "Pressione I para iniciar/pausar o jogo", -1.0f + margin_x, current_y, 1.0f);
        current_y += pad;

        snprintf(buffer, 80, "Checkpoints Faltantes: %lu", g_Checkpoints.size());
        TextRendering_PrintString(window, buffer, -1.0f + margin_x, 1.0f - margin_y_top, 1.0f);
    }
}


// inicia os checkpoints no jogo
void initCheckpoints() {
    g_Checkpoints.clear();
    
    float orbit_distance = 16.0f; 
    glm::vec4 center = moon_position; 
    
    g_Checkpoints.push_back(center + glm::vec4(orbit_distance, 0.0f, 0.0f, 1.0f));  
    g_Checkpoints.push_back(center + glm::vec4(-orbit_distance, 0.0f, 0.0f, 1.0f)); 
    g_Checkpoints.push_back(center + glm::vec4(0.0f, orbit_distance, 0.0f, 1.0f));  
    g_Checkpoints.push_back(center + glm::vec4(0.0f, -orbit_distance, 0.0f, 1.0f)); 
    g_Checkpoints.push_back(center + glm::vec4(0.0f, 0.0f, -orbit_distance, 1.0f)); 
}


// Função que inicializa asteroides em posições aleatórias na órbita
void initRandomAsteroids() {
    g_RandomAsteroids.clear();
    float orbit_distance = 16.0f; 
    glm::vec4 center = moon_position; 

    for (int i = 0; i < numRandomAsteroids; ++i) {
        glm::vec4 randomDir = glm::vec4(
            randomFloat(-1.0f, 1.0f), 
            randomFloat(-1.0f, 1.0f), 
            randomFloat(-1.0f, 1.0f), 
            0.0f
        );
        randomDir = normalizedVec(randomDir);

        glm::vec4 asteroidPos = center + randomDir * orbit_distance;
        asteroidPos.w = 1.0f;

        g_RandomAsteroids.push_back(asteroidPos);
    }
}


// função de tiro do missil
void fireMissile(const glm::vec4& startPos, const glm::vec4& direction, int ownerId) {
    Missile m;
    m.position = startPos;
    m.forward = direction; 
    m.forward.w = 0.0f; 
    m.speed = missileSpeed;
    m.fixedDistance = 16.0f; 
    m.isActive = true;
    m.ownerId = ownerId;
    m.lifeTime = missileLifespan;

    g_Missiles.push_back(m);
}

// atualiza a movimentação do missil
void updateMissiles(float tprev, float tnow) {
    float delta_t = tnow - tprev;
    float fixedDistance = 16.0f; 

    for (auto it = g_Missiles.begin(); it != g_Missiles.end();) {
        Missile &m = *it;

        if (!m.isActive) {
            it = g_Missiles.erase(it);
            continue;
        }

        m.lifeTime -= delta_t;
        if (m.lifeTime <= 0.0f) {
            m.isActive = false;
            it = g_Missiles.erase(it);
            continue;
        }

        glm::vec4 currentPos = m.position;
        glm::vec4 up_vec = normalizedVec(currentPos - moon_position);

        // O míssil avança na direção 'forward'
        glm::vec4 right_vec = normalizedVec(crossproduct(up_vec, m.forward));
        
        // Angulo de avanço na esfera (usando o vetor UP e o vetor RIGHT)
        float angle_of_advance = m.speed * delta_t * 0.05f; 
        glm::mat4 moveMatrix = Matrix_Rotate(angle_of_advance, right_vec); 

        glm::vec4 relativePosition = currentPos - moon_position;
        relativePosition = moveMatrix * relativePosition;
        
        // Reprojetar na distância fixa de órbita
        glm::vec4 finalDir = relativePosition; finalDir.w = 0.0f;
        finalDir = normalizedVec(finalDir);
        m.position = moon_position + (finalDir * fixedDistance);
        m.position.w = 1.0f;
        
        // A direção FORWARD também precisa ser girada para acompanhar a curva.
        m.forward = normalizedVec(moveMatrix * m.forward);

        it++;
    }
}
