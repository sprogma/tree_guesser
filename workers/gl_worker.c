#define SDL_MAIN_HANDLED

#include "D:/c/SDL2/x86_64-w64-mingw32/include/SDL2/SDL.h"
#include "D:/c/GLEW/include/GL/glew.h"

#define __USE_MINGW_ANSI_STDIO 1
#include "../tree.h"
#include "../worker.h"
#include "../akinator_types.h"
#include "assert.h"
#include "time.h"
#include "windows.h"
#include "math.h"
#include "stdio.h"
#include "stdlib.h"


#define ANTIALIASING 4
#define TEST_DEPTH
#define SHADER_POINT_SIZE
// #define WIREFRAME

void APIENTRY glDebugOutput(GLenum source, GLenum type, unsigned int id, GLenum severity, GLsizei length, const char *message, const void *userParam) {
    (void)source;
    (void)type;
    (void)id;
    (void)severity;
    (void)length;
    (void)userParam;
    fprintf(stderr, "Error: %s\n", message);
}

struct point {float x, y, z; float r, g, b; float tx, ty, tz; };

union vector_t
{
    struct { float x, y, z; };
    float v[3];
};

GLuint VBOFone, VAOFone;
GLuint shaderProgramFone;
GLuint whratio_LocationFone;
GLuint time_LocationFone;

SDL_Window *window;
SDL_GLContext *context;
int W, H;

GLuint VBO, VAO, IBO;
GLuint shaderProgram;
GLuint camera_matrix_Location;
GLuint whratio_Location;
GLuint screenh_Location;
GLuint time_Location;
int indexes_len;

union vector_t cam_position = {{-5.0, 0.0, 0.0}};
float cam_yaw = 0.0, cam_pitch = 0.0;
float cam_scale = 0.02;

void draw_and_event(double tim, double dtim);

const char* vertexShaderSource = R"shader(
    #version 460 core


    layout(location = 0) in vec3 position_in_world;
    layout(location = 1) in vec3 in_color;
    layout(location = 2) in vec3 texture_position;

    uniform mat4 camera_matrix;
    uniform float whratio;
    uniform float time;

    out vec3 pos;
    out vec2 screen_pos;
    out vec2 texture_pos;

    void main()
    {
        vec3 world_pos = position_in_world;
        world_pos.y += 3 * sin(2 * time + texture_position.z * 1.2);
        world_pos.x += sin(texture_position.z * 12412.1512) * 50.0 * clamp(6.0/(1.0 + time) - 0.5, 0.0, 1.0);
    
        texture_pos = texture_position.xy;
        
        pos = world_pos;
        vec3 position_in_cam = (vec4(world_pos, 1.0) * camera_matrix).xyz;
        gl_Position = vec4(position_in_cam, 1.0 + position_in_cam.z);
        gl_PointSize = 25.0 / gl_Position.w;
        gl_Position.x /= whratio;
        screen_pos = gl_Position.xy / gl_Position.w;
    }
)shader";


const char* fragmentShaderSource = R"shader(
    #version 460 core

    uniform float time;
    uniform float whratio;
    uniform float screenh;
    
    in vec3 pos;
    in vec2 screen_pos;
    in vec2 texture_pos;
    out vec4 color;
    
    uniform sampler2D textureSampler;

    void main()
    {
        // vec2 txpos = gl_FragCoord.xy / vec2(whratio * screenh, screenh);
        // txpos.y = 1.0 - txpos.y;
        // vec3 ct = texture(textureSampler, txpos).rgb;
        float t = cos(time + pos.x * 0.2)*0.5+0.5;
        vec3 cc = vec3(sin(time * 1.1251251 + pos.x * 0.11)*0.5+0.5, t, 1.0f - t);
        color.xyz = cc;
    }
)shader";


const char* vertexShaderSourceFone = R"shader(
    #version 460 core

    layout(location = 0) in vec3 position_in_world;

    uniform mat4 camera_matrix;
    uniform float whratio;
    uniform float time;

    out vec2 screen_pos;

    void main()
    {
        gl_Position.xy = position_in_world.xy;
        gl_Position.z = -1.0;
        gl_Position.x /= whratio;
        gl_Position.w = 1.0;
        
        screen_pos = gl_Position.xy / gl_Position.w;
    }
)shader";


const char* fragmentShaderSourceFone = R"shader(
    #version 460 core

    uniform float time;
    uniform float whratio;
    
    in vec2 screen_pos;
    out vec4 color;
    
    uniform sampler2D textureSampler;

    void main()
    {
        vec3 cc = vec3(sin(time * 1.1251251 + pos.x * 0.11)*0.5+0.5, cos(time + pos.x * 0.2)*0.5+0.5, 0.0f);
        color.xyz = cc;
        color.w = 1.0;
    }
)shader";

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
// int main(int argc, char **argv)
{
    // (void)argc;
    // (void)argv;
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;
    
    srand(time(NULL));
    
    /* load texture */
    
    
    SDL_Init(SDL_INIT_EVERYTHING);

    #ifdef ANTIALIASING
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, ANTIALIASING);
    #endif

    SDL_DisplayMode DM;
    SDL_GetDesktopDisplayMode(0, &DM);
    int Width, Height;
    Width = DM.w;
    Height = DM.h;

    window = SDL_CreateWindow("Akinator",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              Width,
                              Height,
                            //SDL_WINDOW_FULLSCREEN | 
                              SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
                              
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    context = SDL_GL_CreateContext(window);
    if (context == NULL) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Failed to create OpenGL context: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        printf("%d\n", __LINE__);
        return 1;
    }

    glewExperimental = GL_TRUE;
    GLenum GlewError = glewInit();
    if (GlewError != GLEW_OK) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize GLEW: %s", glewGetErrorString(GlewError));
        printf("%d\n", __LINE__);
        return 1;
    }
    SDL_GL_GetDrawableSize(window, &W, &H);
    glViewport(0, 0, W, H);

    #ifdef TEST_DEPTH
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        glDepthRange(0.0f, 1.0f);
        glEnable(GL_DEPTH_CLAMP);
    #endif
    
    #ifdef ANTIALIASING
        glEnable(GL_MULTISAMPLE);
    #endif

    #ifdef WIREFRAME
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    #endif

    #ifdef SHADER_POINT_SIZE
        glEnable(GL_PROGRAM_POINT_SIZE);
    #endif

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(glDebugOutput, NULL);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);


    /* load texture */
    SDL_Surface *texture = SDL_LoadBMP("./workers/gl_texture.bmp");
    if (!texture)
    {
        printf("%d [%s]\n", __LINE__, SDL_GetError());
        return 1;
    }
    
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);        
    
    int mode = GL_BGR;
    if (texture->format->BytesPerPixel == 4) {
        mode = GL_BGRA;
    }

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        mode,
        texture->w,
        texture->h,
        0,
        mode,
        GL_UNSIGNED_BYTE,
        texture->pixels
    );
    
    SDL_FreeSurface(texture);

    // SDL_GL_SetSwapInterval(0);

    

    double tim = 0, dtim;

    /* create VBOFone */
    glGenBuffers(1, &VBOFone);
    glBindBuffer(GL_ARRAY_BUFFER, VBOFone);
    /* create VAOFone */
    glGenVertexArrays(1, &VAOFone);
    {
        glBindVertexArray(VAOFone);
        /* vertex attributes */
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2, (GLvoid*)0);
        glEnableVertexAttribArray(0);
    }
    {
        GLuint vertexShader, fragmentShader;
        vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSourceFone, NULL);
        glCompileShader(vertexShader);
        fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSourceFone, NULL);
        glCompileShader(fragmentShader);
        shaderProgramFone = glCreateProgram();
        glAttachShader(shaderProgramFone, vertexShader);
        glAttachShader(shaderProgramFone, fragmentShader);
        glLinkProgram(shaderProgramFone);
        glUseProgram(shaderProgramFone);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    camera_matrix_LocationFone = glGetUniformLocation(shaderProgramFone, "camera_matrix");
    time_LocationFone = glGetUniformLocation(shaderProgramFone, "time");
    {
        GLint textureUniformLocationFone = glGetUniformLocation(shaderProgramFone, "textureSampler");
        glUniform1i(textureUniformLocationFone, 0);
    }


    {
        float vhr = (float)W / (float)H;
        
        block_usedFone = 0;

        split_on_triangles(bufferFone, 100, 0.0, 0.0, Fone.0, 0.0, 0.0, Fone.0);
        
        /* gen triangles */
        // bufferFone[0+0] = (struct point){ -vhr, -1.0,  0.0,
        //                                 0.0,  0.0,  0.0,
        //                                 0.0,  0.0,  0.0};
        // bufferFone[0+1] = (struct point){  vhr, -1.0,  0.0,
        //                                 0.0,  0.0,  0.0,
        //                                 1.0,  0.0,  0.0};
        // bufferFone[0+Fone] = (struct point){ -vhr,  1.0,  0.0,
        //                                 0.0,  0.0,  0.0,
        //                                 0.0,  1.0,  0.0};
        // bufferFone[3+0] = (struct point){ -vhr,  1.0,  0.0,
        //                                 0.0,  0.0,  0.0,
        //                                 0.0,  1.0,  1.0};
        // bufferFone[3+1] = (struct point){  vhr,  1.0,  0.0,
        //                                 0.0,  0.0,  0.0,
        //                                 1.0,  1.0,  1.0};
        // bufferFone[3+Fone] = (struct point){  0.0,  0.0,  0.0,
        //                                 0.0,  0.0,  0.0,
        //                                 0.5,  0.5,  1.0};
        // bufferFone[6+0] = (struct point){  vhr, -1.0,  0.0,
        //                                 0.0,  0.0,  0.0,
        //                                 1.0,  0.0,  Fone.0};
        // bufferFone[6+1] = (struct point){  vhr,  1.0,  0.0,
        //                                 0.0,  0.0,  0.0,
        //                                 1.0,  1.0,  Fone.0};
        // bufferFone[6+Fone] = (struct point){  0.0,  0.0,  0.0,
        //                                 0.0,  0.0,  0.0,
        //                                 0.5,  0.5,  Fone.0};

        for (int i = 0; i < block_usedFone; ++i)
        {
            bufferFone[i].x *= vhr;
        }

        glBindVertexArray(VAOFone);
        glBindBuffer(GL_ARRAY_BUFFER, VBOFone);
        glBufferData(GL_ARRAY_BUFFER, sizeof(*bufferFone) * block_usedFone, bufferFone, GL_STREAM_DRAW);
        
        /* set wh ratio */
        {
            whratio_LocationFone = glGetUniformLocation(shaderProgramFone, "whratio");
            glUniform1f(whratio_LocationFone, vhr);
            screenh_LocationFone = glGetUniformLocation(shaderProgramFone, "screenh");
            glUniform1f(screenh_LocationFone, (float)H);
        }
    }    




    /* create VBO */
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    /* create VAO */
    glGenVertexArrays(1, &VAO);
    {
        glBindVertexArray(VAO);
        /* vertex attributes */
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 9, (GLvoid*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 9, (GLvoid*)(sizeof(GLfloat) * 3));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 9, (GLvoid*)(sizeof(GLfloat) * 6));
        glEnableVertexAttribArray(2);
        
    }

    {
        GLuint vertexShader, fragmentShader;
        vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader);
        TEST_COMPILATION(vertexShader)
        fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);
        TEST_COMPILATION(fragmentShader)
        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);
        glUseProgram(shaderProgram);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    camera_matrix_Location = glGetUniformLocation(shaderProgram, "camera_matrix");
    time_Location = glGetUniformLocation(shaderProgram, "time");
    {
        GLint textureUniformLocation = glGetUniformLocation(shaderProgram, "textureSampler");
        glUniform1i(textureUniformLocation, 0);
    }

    int buffer_used = 0;
    struct point buffer[1024] = {};

    buffer_used = 0;
    int indexes_base = 0, buffer_used_x = 0;
    indexes_len = 0;
    Uint32 indexes[1024] = {};
    
    /* create data */
    #define ADD_POINT(x, y, z) \
        buffer[buffer_used++] = (struct point){(x), (y), (z), 0, 0, 0, x, y, NUM};
        
    #define ADD_INDEX(id) \
        indexes[indexes_len++] = indexes_base + id;
        
    #define ADD_INDEZ(id) \
        indexes[indexes_len++] = buffer_used_x + id;
        
    #define ADD_TRING(x, y, z) \
        ADD_INDEX(x) \
        ADD_INDEX(y) \
        ADD_INDEX(z)
        
    #define ADD_TRINB(x, y, z) \
        ADD_INDEZ(x) \
        ADD_INDEZ(y) \
        ADD_INDEZ(z)

    #define ADD_RECTA(x, y, z, w) \
        ADD_TRING(x, y, w) \
        ADD_TRING(w, y, z)

    #define ADD_RECTB(x, y, z, w) \
        ADD_TRINB(x, y, w) \
        ADD_TRINB(w, y, z)

    #define ADD_LINE(a, b) \
        ADD_TRING(a, b, a + buffer_used_x - indexes_base) \
        ADD_TRING(a + buffer_used_x - indexes_base, b, b + buffer_used_x - indexes_base)

    #define ADD_RECTAB(x, y, z, w) \
        ADD_RECTA(x, y, z, w) \
        ADD_RECTB(x, y, z, w)

    #define START \
        indexes_base = buffer_used;
        
    #define INSERT_BACK \
        buffer_used_x = buffer_used; \
        for (int i = indexes_base; i < buffer_used_x; ++i) \
        { \
            buffer[buffer_used] = buffer[i]; \
            buffer[buffer_used].z += 1; \
            buffer_used++; \
        }

    
    #define PRINT_S(dx, dy, dz) \
        START \
        ADD_POINT(dx + 0.0, dy - 4.5, dz - 0.5) \
        ADD_POINT(dx + 0.0, dy - 5.5, dz - 0.5) \
        ADD_POINT(dx - 2.0, dy - 2.5, dz - 0.5) \
        ADD_POINT(dx - 3.0, dy - 2.5, dz - 0.5) \
        ADD_POINT(dx + 2.0, dy - 2.5, dz - 0.5) \
        ADD_POINT(dx + 3.0, dy - 2.5, dz - 0.5) \
        ADD_POINT(dx + 0.0, dy + 4.5, dz - 0.5) \
        ADD_POINT(dx + 0.0, dy + 5.5, dz - 0.5) \
        ADD_POINT(dx - 2.0, dy + 2.5, dz - 0.5) \
        ADD_POINT(dx - 3.0, dy + 2.5, dz - 0.5) \
        ADD_POINT(dx + 2.0, dy + 2.5, dz - 0.5) \
        ADD_POINT(dx + 3.0, dy + 2.5, dz - 0.5) \
        INSERT_BACK \
        ADD_RECTAB(0, 2, 3, 1) \
        ADD_RECTAB(0, 1, 5, 4) \
        ADD_RECTAB(4, 5, 8, 9) \
        ADD_RECTAB(9, 8, 6, 7) \
        ADD_RECTAB(7, 6, 10, 11) \
        ADD_LINE(0, 2) \
        ADD_LINE(2, 3) \
        ADD_LINE(3, 1) \
        ADD_LINE(1, 5) \
        ADD_LINE(5, 8) \
        ADD_LINE(8, 6) \
        ADD_LINE(6, 10) \
        ADD_LINE(10, 11) \
        ADD_LINE(11, 7) \
        ADD_LINE(7, 9) \
        ADD_LINE(9, 4) \
        ADD_LINE(4, 0)

    
    #define PRINT_O(dx, dy, dz) \
        START \
        ADD_POINT(dx + 2.0, dy - 4.5, dz - 0.5) \
        ADD_POINT(dx + 3.0, dy - 5.5, dz - 0.5) \
        ADD_POINT(dx - 2.0, dy - 4.5, dz - 0.5) \
        ADD_POINT(dx - 3.0, dy - 5.5, dz - 0.5) \
        ADD_POINT(dx - 2.0, dy + 4.5, dz - 0.5) \
        ADD_POINT(dx - 3.0, dy + 5.5, dz - 0.5) \
        ADD_POINT(dx + 2.0, dy + 4.5, dz - 0.5) \
        ADD_POINT(dx + 3.0, dy + 5.5, dz - 0.5) \
        INSERT_BACK \
        ADD_RECTAB(0, 1, 3, 2) \
        ADD_RECTAB(2, 3, 5, 4) \
        ADD_RECTAB(4, 5, 7, 6) \
        ADD_RECTAB(6, 7, 1, 0) \
        ADD_LINE(0, 2) \
        ADD_LINE(2, 4) \
        ADD_LINE(4, 6) \
        ADD_LINE(6, 0) \
        ADD_LINE(1, 3) \
        ADD_LINE(3, 5) \
        ADD_LINE(5, 7) \
        ADD_LINE(7, 1)

    
    #define PRINT_I(dx, dy, dz) \
        START \
        ADD_POINT(dx + 0.5, dy - 5.5, dz - 0.5) \
        ADD_POINT(dx - 0.5, dy - 5.5, dz - 0.5) \
        ADD_POINT(dx + 0.5, dy - 4.5, dz - 0.5) \
        ADD_POINT(dx - 0.5, dy - 4.5, dz - 0.5) \
        ADD_POINT(dx + 0.5, dy - 3.5, dz - 0.5) \
        ADD_POINT(dx - 0.5, dy - 3.5, dz - 0.5) \
        ADD_POINT(dx + 0.5, dy + 5.5, dz - 0.5) \
        ADD_POINT(dx - 0.5, dy + 5.5, dz - 0.5) \
        INSERT_BACK \
        ADD_RECTAB(0, 1, 3, 2) \
        ADD_RECTAB(4, 5, 7, 6) \
        ADD_LINE(0, 1) \
        ADD_LINE(1, 3) \
        ADD_LINE(3, 2) \
        ADD_LINE(2, 0) \
        ADD_LINE(4, 5) \
        ADD_LINE(5, 7) \
        ADD_LINE(7, 6) \
        ADD_LINE(6, 4)

    
    #define PRINT_W(dx, dy, dz) \
        START \
        ADD_POINT(dx + 2.0, dy - 4.0, dz - 0.5) \
        ADD_POINT(dx + 0.5, dy - 1.0, dz - 0.5) \
        ADD_POINT(dx - 0.5, dy - 1.0, dz - 0.5) \
        ADD_POINT(dx - 2.0, dy - 4.0, dz - 0.5) \
        ADD_POINT(dx - 3.0, dy - 4.0, dz - 0.5) \
        ADD_POINT(dx - 2.0, dy + 4.0, dz - 0.5) \
        ADD_POINT(dx - 1.0, dy + 4.0, dz - 0.5) \
        ADD_POINT(dx + 1.0, dy + 4.0, dz - 0.5) \
        ADD_POINT(dx + 2.0, dy + 4.0, dz - 0.5) \
        ADD_POINT(dx + 3.0, dy - 4.0, dz - 0.5) \
        INSERT_BACK \
        ADD_RECTAB(5, 6, 3, 4) \
        ADD_RECTAB(5, 6, 1, 2) \
        ADD_RECTAB(7, 8, 9, 0) \
        ADD_RECTAB(7, 8, 1, 2) \
        ADD_LINE(0, 7) \
        ADD_LINE(7, 1) \
        ADD_LINE(1, 2) \
        ADD_LINE(2, 5) \
        ADD_LINE(5, 4) \
        ADD_LINE(4, 3) \
        ADD_LINE(3, 6) \
        ADD_LINE(6, 1) \
        ADD_LINE(1, 8) \
        ADD_LINE(8, 9) \
        ADD_LINE(9, 0) \
        ADD_LINE(5, 6) \
        ADD_LINE(7, 8)

    
    #define PRINT_N(dx, dy, dz) \
        START \
        ADD_POINT(dx + 2.5, dy - 5.5, dz - 0.5) \
        ADD_POINT(dx + 3.5, dy - 4.5, dz - 0.5) \
        ADD_POINT(dx - 2.5, dy - 5.5, dz - 0.5) \
        ADD_POINT(dx - 3.5, dy - 4.5, dz - 0.5) \
        ADD_POINT(dx + 2.5, dy + 5.5, dz - 0.5) \
        ADD_POINT(dx + 3.5, dy + 4.5, dz - 0.5) \
        ADD_POINT(dx - 2.5, dy + 5.5, dz - 0.5) \
        ADD_POINT(dx - 3.5, dy + 4.5, dz - 0.5) \
        INSERT_BACK \
        ADD_RECTAB(0, 1, 5, 4) \
        ADD_RECTAB(5, 4, 3, 2) \
        ADD_RECTAB(3, 2, 6, 7) \
        ADD_LINE(0, 1) \
        ADD_LINE(1, 5) \
        ADD_LINE(5, 4) \
        ADD_LINE(4, 3) \
        ADD_LINE(3, 7) \
        ADD_LINE(7, 6) \
        ADD_LINE(6, 2) \
        ADD_LINE(2, 3) \
        ADD_LINE(2, 5) \
        ADD_LINE(4, 0)

    #define T 8.0

    #define NUM 0
    PRINT_S(-3*T, 0.0, 0.0);
    #undef NUM
    #define NUM 1
    PRINT_W(-2*T, 0.0, 0.0);
    #undef NUM
    #define NUM 2
    PRINT_O(-1*T, 0.0, 0.0);
    #undef NUM
    #define NUM 2.75
    PRINT_O(0*T, -3.0, 0.0);
    #undef NUM
    #define NUM 3.25
    PRINT_O(0*T, 3.0, 0.0);
    #undef NUM
    #define NUM 4
    PRINT_N(1*T, 0.0, 0.0);
    #undef NUM
    #define NUM 5
    PRINT_I(2*T, 0.0, 0.0);
    #undef NUM
    #define NUM 6
    PRINT_W(3*T, 0.0, 0.0);
    #undef NUM


    float S = 1.0;
    for (int i = 0; i < buffer_used; ++i)
    {
        buffer[i].x *= S;
        buffer[i].y *= -S;
        buffer[i].z *= S;
        buffer[i].y += 1.0;
    }
    
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(*buffer) * buffer_used, buffer, GL_STATIC_DRAW); // GL_STREAM_DRAW
    
    glGenBuffers(1, &IBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(*indexes) * indexes_len, indexes, GL_STATIC_DRAW);
    
    /* set wh ratio */
    {
        whratio_Location = glGetUniformLocation(shaderProgram, "whratio");
        glUniform1f(whratio_Location, (float)W / (float)H);
        screenh_Location = glGetUniformLocation(shaderProgram, "screenh");
        glUniform1f(screenh_Location, (float)H);
    }

    while (1)
    {   
        {
            double ptim = tim;
            tim = SDL_GetTicks64() * 0.001;
            dtim = tim - ptim;
        }
        draw_and_event(tim, dtim);
        Sleep(11);
    }
}

void draw_and_event(double tim, double dtim)
{
    // SDL_SetWindowSize(window, W, H);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    

    glBindVertexArray(VAO2);
    glUseProgram(shaderProgram2);

    glUniform1f(time_Location2, tim - 3.5);
    glDrawArrays(GL_TRIANGLES, 0, block_used2);

    if (tim > 3.5)
    {
        double t = tim - 3.5;
        srand(0);
        for (int i = 0; i < block_used2; i += 3)
        {
            float dx = (rand() % 11 - 5) * 0.002 * 1.0 / (1.0 + t);
            float dy = (rand() % 11 - 8) * 0.003 + t * t * 0.007;
            // dx *= 0.03;
            // dy *= 0.03;
            buffer2[i+0].x += dx;
            buffer2[i+0].y += dy;
            buffer2[i+1].x += dx;
            buffer2[i+1].y += dy;
            buffer2[i+2].x += dx;
            buffer2[i+2].y += dy;

            /* rotate triangle */
            double cx = (buffer2[i+0].x + buffer2[i+1].x + buffer2[i+2].x) * 0.333333333;
            double cy = (buffer2[i+0].y + buffer2[i+1].y + buffer2[i+2].y) * 0.333333333;
            double cz = (buffer2[i+0].z + buffer2[i+1].z + buffer2[i+2].z) * 0.333333333;
            buffer2[i+0].x -= cx;
            buffer2[i+0].y -= cy;
            buffer2[i+0].z -= cz;
            buffer2[i+1].x -= cx;
            buffer2[i+1].y -= cy;
            buffer2[i+1].z -= cz;
            buffer2[i+2].x -= cx;
            buffer2[i+2].y -= cy;
            buffer2[i+2].z -= cz;

            double angle = ((double)rand() / RAND_MAX) * 0.025 + 0.0025;

            double ux = (double)rand() / RAND_MAX * 2.0 - 1.0;
            double uy = (double)rand() / RAND_MAX * 2.0 - 1.0;
            double uz = (double)rand() / RAND_MAX * 2.0 - 1.0;
            double inv_len = 1.0 / sqrt(ux*ux + uy*uy + uz*uz);
            ux *= inv_len; uy *= inv_len; uz *= inv_len;

            double c = cos(angle);
            double s = sin(angle);
            double zz = 1.0 - c;

            double a[3][3];
            a[0][0] = zz*ux*ux + c;
            a[0][1] = zz*ux*uy - s*uz;
            a[0][2] = zz*ux*uz + s*uy;

            a[1][0] = zz*ux*uy + s*uz;
            a[1][1] = zz*uy*uy + c;
            a[1][2] = zz*uy*uz - s*ux;

            a[2][0] = zz*ux*uz - s*uy;
            a[2][1] = zz*uy*uz + s*ux;
            a[2][2] = zz*uz*uz + c;

            double tx, ty, tz;

            tx = a[0][0]*buffer2[i+0].x + a[0][1]*buffer2[i+0].y + a[0][2]*buffer2[i+0].z;
            ty = a[1][0]*buffer2[i+0].x + a[1][1]*buffer2[i+0].y + a[1][2]*buffer2[i+0].z;
            tz = a[2][0]*buffer2[i+0].x + a[2][1]*buffer2[i+0].y + a[2][2]*buffer2[i+0].z;
            buffer2[i+0].x = tx; buffer2[i+0].y = ty; buffer2[i+0].z = tz;

            tx = a[0][0]*buffer2[i+1].x + a[0][1]*buffer2[i+1].y + a[0][2]*buffer2[i+1].z;
            ty = a[1][0]*buffer2[i+1].x + a[1][1]*buffer2[i+1].y + a[1][2]*buffer2[i+1].z;
            tz = a[2][0]*buffer2[i+1].x + a[2][1]*buffer2[i+1].y + a[2][2]*buffer2[i+1].z;
            buffer2[i+1].x = tx; buffer2[i+1].y = ty; buffer2[i+1].z = tz;

            tx = a[0][0]*buffer2[i+2].x + a[0][1]*buffer2[i+2].y + a[0][2]*buffer2[i+2].z;
            ty = a[1][0]*buffer2[i+2].x + a[1][1]*buffer2[i+2].y + a[1][2]*buffer2[i+2].z;
            tz = a[2][0]*buffer2[i+2].x + a[2][1]*buffer2[i+2].y + a[2][2]*buffer2[i+2].z;
            buffer2[i+2].x = tx; buffer2[i+2].y = ty; buffer2[i+2].z = tz;

            buffer2[i+0].x += cx;
            buffer2[i+0].y += cy;
            buffer2[i+0].z += cz;
            buffer2[i+1].x += cx;
            buffer2[i+1].y += cy;
            buffer2[i+1].z += cz;
            buffer2[i+2].x += cx;
            buffer2[i+2].y += cy;
            buffer2[i+2].z += cz;
        }
    }

    if (tim > 2)
    {
        double zz = tim - 2.0;
        glBindVertexArray(VAO2);
        glBindBuffer(GL_ARRAY_BUFFER, VBO2);
        glBufferData(GL_ARRAY_BUFFER, sizeof(*buffer2) * block_used2, buffer2, GL_STREAM_DRAW);

        glBindVertexArray(VAO);
        glUseProgram(shaderProgram);
        
        union vector_t r, u, f, c;
        float PI2 = 1.5707963267949;
        r = (union vector_t){{cos(cam_yaw + PI2), 0.0, sin(cam_yaw + PI2)}};
        u = (union vector_t){{cos(cam_yaw) * cos(cam_pitch + PI2), sin(cam_pitch + PI2), sin(cam_yaw) * cos(cam_pitch + PI2)}};
        f = (union vector_t){{cos(cam_yaw) * cos(cam_pitch), sin(cam_pitch), sin(cam_yaw) * cos(cam_pitch)}};
        c = cam_position;
        // right, up, forward, cam_position
        if (zz > 1)
        {
            cam_scale = 0.02 * (1 + 100 / (100 * (zz - 1)/1.8 + 1));
        }
        else
        {
            cam_scale = 0.02 * 101;
        }
        r.x *= cam_scale; r.y *= cam_scale; r.z *= cam_scale;
        u.x *= cam_scale; u.y *= cam_scale; u.z *= cam_scale;
        f.x *= cam_scale; f.y *= cam_scale; f.z *= cam_scale;
        
        float matrix[16] = {
            r.x,r.y,r.z,-c.x*r.x-c.y*r.y-c.z*r.z,
            u.x,u.y,u.z,-c.x*u.x-c.y*u.y-c.z*u.z,
            f.x,f.y,f.z,-c.x*f.x-c.y*f.y-c.z*f.z,
            0,0,0,1,
        };

        glUniform1f(time_Location, zz);
        glUniformMatrix4fv(camera_matrix_Location, 1, GL_FALSE, matrix);
        
        glDrawElements(GL_TRIANGLES, indexes_len, GL_UNSIGNED_INT, 0);
    }
    
    glBindVertexArray(0);

    SDL_GL_SwapWindow(window);

    if (SDL_GetWindowFlags(window) & SDL_WINDOW_HIDDEN)
    {
        SDL_SetWindowFullscreen(window, 1);
        SDL_ShowWindow(window);
    }
    
    // cam_yaw += 0.02 * e.motion.xrel;
    // cam_pitch += 0.02 * -e.motion.yrel;
    cam_yaw += 1.05 * dtim;
    cam_pitch = 0.5 * sin(tim * 0.975172) * sin(tim * 0.17512);
    // cam_pitch = 0;

    cam_position.x = 0;
    cam_position.y = 0;
    cam_position.z = 0;

    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT)
        {
            SDL_Quit();
            exit(0);
        }
        if (e.type == SDL_KEYDOWN)
        {
            SDL_Quit();
            exit(0);
        }
        if (e.type == SDL_MOUSEBUTTONDOWN)
        {
            SDL_Quit();
            exit(0);
        }
        if (e.type == SDL_MOUSEMOTION && no_exit_on_motion)
        {
            if (e.motion.xrel != 0 || e.motion.yrel != 0)
            {
                SDL_Quit();
                exit(0);
            }
        }
    }
}













#define lock_log(...)


void akinator_ask_question(struct worker_instance *wk, struct worker_task *tsk)
{
    struct akinator_task_data *data = tsk->data;
    
    /* get node from iterator */
    int64_t node_id = tree_iterator_get_node(data->iterator, 0);

    if (node_id == INVALID_NODE_ID || data->dont_solved)
    {
        char *str = malloc(1000);
        sprintf(str, "Я не знаю кто это. Плак плак.");
        worker_pool_send_event(wk, str);
        
        str = malloc(1000);
        sprintf(str, "Введи что это, чтобы я знал - это:");
        worker_pool_send_event(wk, str);

        data->dont_solved = 1;
        worker_pool_wait_event(wk);
    }
    
    struct node_allocator *allocator = tree_get_allocator(data->iterator->tree);
    struct node *node = allocator_acquire_node(allocator, node_id, 0);

    if (node->type == NODE_LEAF)
    {
        struct node_leaf *leaf = (struct node_leaf *)node;
        
        char *str = malloc(1000);
        sprintf(str, "Ха, это ведь очевидно, это %s?", leaf->record.name);
        worker_pool_send_event(wk, str);
        allocator_release_node(allocator, node, 0);
        worker_pool_wait_event(wk);
    }
    else
    {
        struct node_variant *variant = (struct node_variant *)node;

        char *str = malloc(1000);
        sprintf(str, "Хммммм, а это %s", variant->question.text);
        worker_pool_send_event(wk, str);
        allocator_release_node(allocator, node, 0);
        worker_pool_wait_event(wk);
    }
}


void akinator_worker(struct worker_instance *wk, struct worker_task *tsk, void *event)
{
    struct akinator_task_data *data = tsk->data;

    if (data->add_new_name != NULL)
    {

        if (strlen(event) + 1 > sizeof(((struct question *)NULL)->text))
        {
            char *str = malloc(1000);
            sprintf(str, "СЛИШКОМ МНОГО БУКАВ! лень читать. напиши покороче");
            worker_pool_send_event(wk, str);
            
            free(event);
            worker_pool_wait_event(wk);
        }
        
        char *str = malloc(1000);
        sprintf(str, "Да, теперь припоминаю...");
        worker_pool_send_event(wk, str);
        
        struct tree_split_node_result res = tree_split_node(data->iterator->tree, data->iterator, 1, event);

        /* and add new record */

        struct tree_iterator *it = tree_iterator_create(data->iterator->tree, res.version_id);
        for (int i = 0; i + 1 < data->iterator->path_len; ++i)
        {
            int64_t n_id = tree_iterator_get_node(data->iterator, data->iterator->path_len - i - 2);
            int64_t x_id = tree_iterator_get_node(data->iterator, data->iterator->path_len - i - 1);
            struct node_allocator *allocator = tree_get_allocator(data->iterator->tree);
            struct node_variant *x = (struct node_variant *)allocator_acquire_node(allocator, x_id, 0);
            assert(x->type == NODE_VARIANT);
            if (n_id == x->l)
            {
                tree_iterator_try_move_down(it, 1);
            }
            else
            {
                tree_iterator_try_move_down(it, 0);
            }
            allocator_release_node(allocator, (struct node *)x, 0);
        }

        assert(tree_iterator_get_node(it, 0) == res.new_node_id);
        
        tree_set_leaf(it->tree, it, 0, event);
        
        free(data->add_new_name);
        data->add_new_name = NULL;

        tree_iterator_free(it);
        
        free(event);
        worker_pool_exit(wk);
    }
    else if (event == NULL)
    {
        char *str = malloc(1000);
        sprintf(str, "Привет юзер %lld! Загадай что нибудь, а потом отвечай на вопросы - я угадаю что это.", data->user->id);
        worker_pool_send_event(wk, str);
        
        akinator_ask_question(wk, tsk);
    }
    else if (data->dont_solved)
    {
        if (strlen(event) + 1 > sizeof(((struct question *)NULL)->text))
        {
            char *str = malloc(1000);
            sprintf(str, "СЛИШКОМ МНОГО БУКАВ! Я не запомню. Напиши что это другими словами");
            worker_pool_send_event(wk, str);
            
            free(event);
            worker_pool_wait_event(wk);
        }
        
        /* update tree */
        char *str = malloc(1000);
        sprintf(str, "А, да, теперь припоминаю, было там что то такое. Ну ладно.");
        worker_pool_send_event(wk, str);
        
        /* add new node */
        {
            int64_t node_id = tree_iterator_get_node(data->iterator, 0);
        
            if (node_id == INVALID_NODE_ID)
            {
                /* this is empty tree - add new character */
                tree_set_leaf(data->iterator->tree, data->iterator, 0, event);
            }
            else
            {
                struct node_allocator *allocator = tree_get_allocator(data->iterator->tree);
                struct node *node = allocator_acquire_node(allocator, node_id, 0);
                
                if (node->type == NODE_LEAF)
                {
                    /* need to split node */
                    /* ask for new question */
                    data->add_new_name = event;
                    
                    char *str = malloc(1000);
                    sprintf(str, "Это ладно, вот какой вопрос: %s это не %s потому что %s ..?\n", data->add_new_name, ((struct node_leaf *)node)->record.name, data->add_new_name);
                    worker_pool_send_event(wk, str);
                    
                    allocator_release_node(allocator, node, 0);
                    worker_pool_wait_event(wk);
                }
                else
                {
                    allocator_release_node(allocator, node, 0);
                    tree_set_leaf(data->iterator->tree, data->iterator, data->set_as_leaf, event);
                }
            }
        }
        
        free(event);
        worker_pool_exit(wk);
    }

    int64_t event_value = atoi((char *)event);
    free(event);

    /* else - move iterator */
    int64_t node_id = tree_iterator_get_node(data->iterator, 0);

    if (node_id == INVALID_NODE_ID)
    {
        fprintf(stderr, "Internal akinator_worker error\n");
        worker_pool_exit(wk);
    }

    lock_log("UPDATE USING ANSWER\n");
    struct node_allocator *allocator = tree_get_allocator(data->iterator->tree);
    struct node *node = allocator_acquire_node(allocator, node_id, 0);

    
    if (node->type == NODE_LEAF)
    {
        struct node_leaf *leaf = (struct node_leaf *)node;

        (void)leaf;

        if (event_value == 1)
        {
            char *str = malloc(1000);
            sprintf(str, "Я победил, потому что я великий компьютерный мозг. Не вини себя 🤓\n");
            worker_pool_send_event(wk, str);
            allocator_release_node(allocator, node, 0);
            worker_pool_exit(wk);
        }
        else
        {
            data->dont_solved = 1;
            // while (tree_iterator_get_node(data->iterator, 0) != INVALID_NODE_ID)
            // {
            //     tree_iterator_move_up(data->iterator);
            // }
            allocator_release_node(allocator, node, 0);
            akinator_ask_question(wk, tsk);
        }   
    }
    else
    {
        struct node_variant *variant = (struct node_variant *)node;
        (void)variant;

        if (event_value == 1)
        {
            if (tree_iterator_try_move_down(data->iterator, 0) != 0)
            {
                data->set_as_leaf = 0;
                data->dont_solved = 1;
            }
        }
        else if (event_value == 0)
        {
            if (tree_iterator_try_move_down(data->iterator, 1) != 0)
            {
                data->set_as_leaf = 1;
                data->dont_solved = 1;
            }
        }
    }

    
    allocator_release_node(allocator, node, 0);
    akinator_ask_question(wk, tsk); 
}


void akinator_worker_send(struct worker_task *tsk, void *event, void *send_data)
{
    (void)tsk;
    (void)send_data;
    printf("AKINATOR: >>> %s\n", (char *)event);
    free(event);
}

void *akinator_worker_prompt()
{
    // lock_log("ANSWER: ");
    char *text = malloc(1000);
    {
        int c, id = 0;
        while ((c = getchar()) != '\n')
        {
            text[id++] = c;
        }
        text[id++] = 0;
    }
    return text;
}

struct akinator_user *akinator_worker_connect_user(struct tree *tree)
{
    (void)tree;
    
    struct akinator_user *user = malloc(sizeof(*user));

    user->id = rand();
    printf("Юзер %lld зачем то подключился\n", user->id);
    
    return user;
}

struct akinator_task_data *akinator_worker_start_game(struct akinator_user *user, struct tree *tree)
{
    struct akinator_task_data *task_data = malloc(sizeof(*task_data));
    
    task_data->dont_solved = 0;
    task_data->user = user;
    task_data->iterator = tree_iterator_create(tree, tree->versions_len - 1);
    task_data->add_new_name = NULL;

    return task_data;
}

void akinator_worker_finalize_game(struct akinator_task_data *task_data, struct akinator_user *user, struct tree *tree)
{
    (void)user;
    (void)tree;
    
    printf("Игрулька закончилась.\n");
    
    if (task_data->add_new_name)
    {
        free(task_data->add_new_name);
    }

    tree_iterator_free(task_data->iterator);

    free(task_data);
}

int32_t akinator_worker_restart_game(struct akinator_user *user, struct tree *tree)
{
    (void)user;
    (void)tree;
    printf("Хочешь ещё разок? [y/n] ");
    char c;
    scanf("%c", &c);
    
    while (getchar() != '\n') {}
    
    return c == 'y' || c == 'Y';
}

void akinator_worker_disconnect_user(struct akinator_user *user, struct tree *tree)
{
    (void)tree;
    
    printf("Юзер %lld зачем то отключился.\n", user->id);
    free(user);
}
