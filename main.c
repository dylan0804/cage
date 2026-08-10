#include <OpenGL/gl3.h>
#include <stddef.h>
#include <stdlib.h>
#define GLFW_INCLUDE_NONE
#include "external/GLFW/glfw3.h"
#include <stdbool.h>
#include <stdio.h>

#define CELL_SIZE 40
#define SAND_SIZE 4
#define GRID_W (640 / SAND_SIZE)
#define GRID_H (480 / SAND_SIZE)
#define MAX_VERTS (GRID_W * GRID_H * 6)

static double last_move = 0;

int L_piece[3][2] = {
    {1, 0},
    {1, 0},
    {1, 1},
};

typedef enum { EMPTY, SAND } CellType;
CellType grid[GRID_H][GRID_W];

typedef struct Piece {
  float x;
  float y;
} Piece;

typedef struct State {
  int width;
  int height;
} State;

typedef struct Renderer {
  GLuint vao;
  GLuint vbo;
  GLuint ebo;
  GLuint shader_program;
  GLFWwindow *window;
  State *state;
} Renderer;

typedef struct Vertex {
  float pos[2];
  float col[3];
} Vertex;

Vertex batch[MAX_VERTS];

unsigned int indices[] = {0, 1, 3, 1, 2, 3};

static const char *vertex_shader_source =
    "#version 330 core\n"
    "layout (location = 0) in vec2 aPos;\n"
    "layout (location = 1) in vec3 aCol;\n"

    "out vec3 color;\n"

    "void main()\n"
    "{\n"
    "   gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);\n" // 2d not 3d
    "   color = aCol;\n"
    "}\n";

static const char *fragment_shader_source = "#version 330 core\n"
                                            "in vec3 color;\n"
                                            "out vec4 FragColor;\n"
                                            "void main()\n"
                                            "{\n"
                                            "FragColor = vec4(color, 1.0f);\n"
                                            "}\n";

void error_callback(int error, const char *description) {
  fprintf(stderr, "Error: %s\n", description);
}

void key_callback(GLFWwindow *window, int key, int scancode, int action,
                  int mods) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwDestroyWindow(window);
    glfwTerminate();
    exit(EXIT_SUCCESS);
  }
}

void draw_rect(float width, float height, float x, float y, Renderer *renderer,
               GLFWwindow *window) {
  State *state = glfwGetWindowUserPointer(window);

  // normalize x and y
  float x2 = x + width;
  float y2 = y + height;

  float ndc_x = (x / state->width) * 2.0 - 1.0;
  float ndc_y = 1.0 - (y / state->height) * 2.0;

  float ndc_x2 = (x2 / state->width) * 2.0 - 1.0;
  float ndc_y2 = 1.0 - (y2 / state->height) * 2.0;

  Vertex vertices[] = {{{ndc_x, ndc_y}, {1.f, 0.f, 1.f}},
                       {{ndc_x2, ndc_y}, {1.f, 0.f, 1.f}},
                       {{ndc_x2, ndc_y2}, {1.f, 0.f, 1.f}},
                       {{ndc_x, ndc_y2}, {1.f, 0.f, 1.f}}};

  glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(Vertex), vertices);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

Renderer *renderer_init(Renderer *renderer, State *state) {
  glfwSetErrorCallback(error_callback);

  if (!glfwInit())
    exit(EXIT_FAILURE);

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  GLFWwindow *window = glfwCreateWindow(640, 480, "Cage", NULL, NULL);
  if (!window) {
    glfwTerminate();
    exit(EXIT_FAILURE);
  }

  glfwSetKeyCallback(window, key_callback);
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1); // vsync -> prevents screen tearing

  // framebuffer size is for the viewport (actual pixels, 2x on retina)
  int fb_width, fb_height;
  glfwGetFramebufferSize(window, &fb_width, &fb_height);
  glViewport(0, 0, fb_width, fb_height);

  // window size is the logical size (640x480) used for all game logic
  int width, height;
  glfwGetWindowSize(window, &width, &height);

  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  renderer->vao = vao;

  // vertex buffer objects (vbo)
  // this is a chunk of memory that lives in the GPU. this is why VRAM is
  // important. more VRAM -> more vertex data, textures, etc
  GLuint vertex_buffer;
  glGenBuffers(1, &vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(Vertex), NULL, GL_DYNAMIC_DRAW);
  renderer->vbo = vertex_buffer;

  // ebo
  GLuint ebo;
  glGenBuffers(1, &ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
               GL_STATIC_DRAW);
  renderer->ebo = ebo;

  const GLuint vertex_shader =
      glCreateShader(GL_VERTEX_SHADER); // create shader object
  glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
  glCompileShader(vertex_shader);

  // fragment shader
  const GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
  glCompileShader(fragment_shader);

  // create shader program
  const GLuint shader_program = glCreateProgram();
  // attach and link the vertex and fragment shader
  glAttachShader(shader_program, vertex_shader);
  glAttachShader(shader_program, fragment_shader);
  glLinkProgram(shader_program);
  renderer->shader_program = shader_program;

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, col));
  glEnableVertexAttribArray(1);

  state->height = height;
  state->width = width;
  glfwSetWindowUserPointer(window, state);

  renderer->window = window;
  renderer->state = state;

  return renderer;
}

void crumble(Piece *piece) {
  int cells_per_block = CELL_SIZE / SAND_SIZE;
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 2; col++) {
      if (L_piece[row][col] == 1) {
        int base_x = (piece->x + col * CELL_SIZE) / SAND_SIZE;
        int base_y = (piece->y + row * CELL_SIZE) / SAND_SIZE;
        for (int sy = 0; sy < cells_per_block; sy++) {
          for (int sx = 0; sx < cells_per_block; sx++) {
            int gx = base_x + sx;
            int gy = base_y + sy;
            if (gx >= 0 && gx < GRID_W && gy >= 0 && gy < GRID_H)
              grid[gy][gx] = SAND;
          }
        }
      }
    }
  }
}

bool collide(Piece *piece, State *state) {
  int row_index = ((int)piece->y + 3 * CELL_SIZE) / SAND_SIZE;
  if (row_index >= GRID_H)
    return true;

  CellType *last_row = grid[row_index];
  int start_col = (int)piece->x / SAND_SIZE;
  int last_col = ((int)piece->x + 2 * CELL_SIZE) / SAND_SIZE;
  for (int i = start_col; i < last_col; i++) {
    if (i >= 0 && i < GRID_W && last_row[i] == SAND)
      return true;
  }
  return false;
}

void spawn_piece(Piece *piece, Renderer *renderer, GLFWwindow *window) {
  State *state = glfwGetWindowUserPointer(window);
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 2; col++) {
      if (L_piece[row][col] == 1) {
        draw_rect(CELL_SIZE, CELL_SIZE, piece->x + col * CELL_SIZE,
                  piece->y + row * CELL_SIZE, renderer, window);
      }
    }
  }
  if (collide(piece, state)) {
    crumble(piece);
    piece->x = rand() % (state->width - CELL_SIZE * 2);
    piece->y = 0;
  }
  double now = glfwGetTime();
  if (piece->y + 3 * CELL_SIZE < state->height) {
    piece->y += 12;
  }
}

void update_sand() {
  for (int row = GRID_H - 2; row >= 0; row--) {
    for (int col = 0; col < GRID_W; col++) {
      if (grid[row][col] == SAND) {
        if (grid[row + 1][col] == EMPTY) {
          grid[row + 1][col] = SAND;
          grid[row][col] = EMPTY;
        } else {
          int dir = (rand() % 2) ? 1 : -1;
          if (col + dir >= 0 && col + dir < GRID_W &&
              grid[row + 1][col + dir] == EMPTY) {
            grid[row + 1][col + dir] = SAND;
            grid[row][col] = EMPTY;
          } else if (col - dir >= 0 && col - dir < GRID_W &&
                     grid[row + 1][col - dir] == EMPTY) {
            grid[row + 1][col - dir] = SAND;
            grid[row][col] = EMPTY;
          }
        }
      }
    }
  }
}

void draw_grid(Renderer *renderer, GLFWwindow *window) {
  for (int row = 0; row < GRID_H; row++) {
    for (int col = 0; col < GRID_W; col++) {
      if (grid[row][col] == SAND) {
        draw_rect(SAND_SIZE, SAND_SIZE, col * SAND_SIZE, row * SAND_SIZE,
                  renderer, window);
      }
    }
  }
}

int main() {
  State st = {0};
  Renderer renderer = {0};
  renderer_init(&renderer, &st);
  State *state = renderer.state;
  Piece piece = {.x = rand() % (state->width - CELL_SIZE * 2), .y = 0};
  while (!glfwWindowShouldClose(renderer.window)) {
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(renderer.shader_program);
    glBindVertexArray(renderer.vao);

    spawn_piece(&piece, &renderer, renderer.window);

    update_sand();
    draw_grid(&renderer, renderer.window);

    glfwSwapBuffers(renderer.window);
    glfwPollEvents();
  }

  glfwDestroyWindow(renderer.window);
  glfwTerminate();
  exit(EXIT_SUCCESS);
}
