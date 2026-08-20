#include <OpenGL/gl3.h>
#include <stddef.h>
#include <stdlib.h>
#define GLFW_INCLUDE_NONE
#include "external/GLFW/glfw3.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define CELL_SIZE 40
#define SAND_SIZE 4
#define GRID_W (640 / SAND_SIZE)
#define GRID_H (960 / SAND_SIZE)
#define MAX_VERTS (GRID_W * GRID_H * 6)
#define MAX_STACK (GRID_W * GRID_H * 4)
#define BLINK_DURATION 0.4

double clear_start_time = -1;

typedef enum { EMPTY, SAND } CellType;
typedef struct {
  CellType cell_type;
  float color[3];
  int piece_type;
  bool clearing;
} Cell;
Cell cells[GRID_H][GRID_W] = {0};

typedef struct {
  int row;
  int col;
} Coord;
Coord stack[MAX_STACK];
int stack_index = 0;

bool visited[GRID_H][GRID_W] = {false};

typedef enum {
  PIECE_I,
  PIECE_O,
  PIECE_T,
  // PIECE_S,
  // PIECE_Z,
  // PIECE_J,
  // PIECE_L,
  PIECE_COUNT
} PieceType;
float piece_colors[PIECE_COUNT][3] = {
    [PIECE_I] = {0.0f, 1.0f, 1.0f}, // cyan
    [PIECE_O] = {1.0f, 1.0f, 0.0f}, // yellow
    [PIECE_T] = {0.6f, 0.0f, 0.8f}, // purple
                                    // [PIECE_S] = {0.0f, 1.0f, 0.0f}, // green
                                    // [PIECE_Z] = {1.0f, 0.0f, 0.0f}, // red
                                    // [PIECE_J] = {0.0f, 0.0f, 1.0f}, // blue
                                    // [PIECE_L] = {1.0f, 0.5f, 0.0f}, // orange
};
int piece_shape[PIECE_COUNT][3][2] = {
    [PIECE_I] = {{1, 0}, {1, 0}, {1, 0}},
    [PIECE_O] = {{1, 1}, {1, 1}, {0, 0}},
    [PIECE_T] = {{1, 0}, {1, 1}, {1, 0}},
    // [PIECE_S] = {{1, 0}, {1, 1}, {0, 1}},
    // [PIECE_Z] = {{0, 1}, {1, 1}, {1, 0}}, [PIECE_J] = {{0, 1}, {0, 1}, {1,
    // 1}}, [PIECE_L] = {{1, 0}, {1, 0}, {1, 1}},
};
int dir[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

typedef struct Piece {
  float x;
  float y;
  PieceType piece_type;
} Piece;

typedef struct State {
  int width;
  int height;
  Piece current_piece;
} State;

typedef struct VertexRenderer {
  GLuint vao;
  GLuint vbo;
  GLuint shader_program;
} VertexRenderer;

typedef struct TextRenderer {
  GLuint vao;
  GLuint vbo;
  GLuint shader_program;
} TextRenderer;

typedef struct Renderer {
  TextRenderer text_renderer;
  VertexRenderer vertex_renderer;
  GLFWwindow *window;
} Renderer;

typedef struct Vertex {
  float pos[2];
  float col[3];
} Vertex;

Vertex batch[MAX_VERTS];
int batch_index = 0;

typedef struct Character {
  unsigned int texture_id;
  int size[2];
  int bearing[2];
  unsigned int advance;
} Character;
Character characters[128];

typedef struct TextVertex {
  float pos[2];
  float tex[2];
} TextVertex;

static const char *text_vertex_shader_source =
    "#version 330 core\n"
    "layout (location = 0) in vec2 aPos;\n"
    "layout (location = 1) in vec2 aTex;\n"

    "out vec2 texCoords;\n"
    "uniform mat4 projection;\n"

    "void main()\n"
    "{\n"
    "   gl_Position = projection * vec4(aPos, 0.0, 1.0);\n"
    "   texCoords = aTex;\n"
    "}\n";

static const char *text_fragment_shader_source =
    "#version 330 core\n"
    "in vec2 texCoords;\n"
    "out vec4 color;\n"

    "uniform sampler2D text;\n"
    "uniform vec3 text_color;\n"

    "void main()\n"
    "{\n"
    "   vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, texCoords).r);\n"
    "   color = vec4(text_color, 1.0) * sampled;"
    "}\n";

static const char *vertex_shader_source =
    "#version 330 core\n"
    "layout (location = 0) in vec2 aPos;\n"
    "layout (location = 1) in vec3 aCol;\n"
    "layout (location = 2) in vec4 vertex;\n"

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
  State *state = glfwGetWindowUserPointer(window);
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwDestroyWindow(window);
    glfwTerminate();
    exit(EXIT_SUCCESS);
  }

  if (key == GLFW_KEY_DOWN && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
    state->current_piece.y += 12;
  }

  if (key == GLFW_KEY_LEFT && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
    if (state->current_piece.x - 10 < 0) {
      state->current_piece.x = 0;
    } else {
      state->current_piece.x -= 10;
    }
  }

  if (key == GLFW_KEY_RIGHT &&
      (action == GLFW_PRESS || action == GLFW_REPEAT)) {
    if (state->current_piece.x + 10 >= state->width - CELL_SIZE * 2) {
      state->current_piece.x = state->width - CELL_SIZE * 2;
    } else {
      state->current_piece.x += 10;
    }
  }
}

typedef float Mat4Row[4];
Mat4Row *ortho(int left, int right, int bottom, int top) {
  float (*result)[4] = calloc(4, sizeof(Mat4Row));

  result[0][0] = 2.0 / (right - left);
  result[1][1] = 2.0 / (top - bottom);
  result[2][2] = -1.0;
  result[3][0] = -1.0 * (right + left) / (right - left);
  result[3][1] = -1.0 * (top + bottom) / (top - bottom);
  result[3][3] = 1.0;

  return result;
}

void push_quad(float width, float height, float x, float y, GLFWwindow *window,
               float piece_color[3]) {
  State *state = glfwGetWindowUserPointer(window);

  float x2 = x + width;
  float y2 = y + height;

  float ndc_x = (x / state->width) * 2.0 - 1.0;
  float ndc_y = 1.0 - (y / state->height) * 2.0;

  float ndc_x2 = (x2 / state->width) * 2.0 - 1.0;
  float ndc_y2 = 1.0 - (y2 / state->height) * 2.0;

  Vertex vertices[] = {
      {{ndc_x, ndc_y}, {piece_color[0], piece_color[1], piece_color[2]}},
      {{ndc_x2, ndc_y}, {piece_color[0], piece_color[1], piece_color[2]}},
      {{ndc_x, ndc_y2}, {piece_color[0], piece_color[1], piece_color[2]}},
      {{ndc_x2, ndc_y}, {piece_color[0], piece_color[1], piece_color[2]}},
      {{ndc_x2, ndc_y2}, {piece_color[0], piece_color[1], piece_color[2]}},
      {{ndc_x, ndc_y2}, {piece_color[0], piece_color[1], piece_color[2]}}};

  memcpy(&batch[batch_index], vertices, sizeof(vertices));
  batch_index += 6;
}

void window_init(Renderer *renderer, State *state) {
  glfwSetErrorCallback(error_callback);

  if (!glfwInit())
    exit(EXIT_FAILURE);

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  GLFWwindow *window = glfwCreateWindow(640, 960, "Cage", NULL, NULL);
  if (!window) {
    glfwTerminate();
    exit(EXIT_FAILURE);
  }

  glfwSetKeyCallback(window, key_callback);
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1); // vsync -> prevents screen tearing

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // framebuffer size is for the viewport (actual pixels, 2x on retina)
  int fb_width, fb_height;
  glfwGetFramebufferSize(window, &fb_width, &fb_height);
  glViewport(0, 0, fb_width, fb_height);

  // window size is the logical size (640x960) used for all game logic
  int width, height;
  glfwGetWindowSize(window, &width, &height);

  glfwSetWindowUserPointer(window, state);

  state->height = height;
  state->width = width;

  renderer->window = window;
}

void vertex_renderer_init(Renderer *renderer, State *state) {
  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  renderer->vertex_renderer.vao = vao;

  // vertex buffer objects (vbo)
  // this is a chunk of memory that lives in the GPU. this is why VRAM is
  // important. more VRAM -> more vertex data, textures, etc
  GLuint vertex_buffer;
  glGenBuffers(1, &vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, MAX_VERTS * sizeof(Vertex), NULL,
               GL_DYNAMIC_DRAW);
  renderer->vertex_renderer.vbo = vertex_buffer;

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
  renderer->vertex_renderer.shader_program = shader_program;

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, col));
  glEnableVertexAttribArray(1);

  glBindVertexArray(renderer->vertex_renderer.vao);
}

void text_renderer_init(Renderer *renderer, State *state) {
  FT_Library ft;
  if (FT_Init_FreeType(&ft)) {
    printf("error init freetype\n");
    return;
  }

  FT_Face face;
  if (FT_New_Face(ft, "./arial.ttf", 0, &face)) {
    printf("failed to load font\n");
    return;
  }

  FT_Set_Pixel_Sizes(face, 0, 48);

  if (FT_Load_Char(face, 'X', FT_LOAD_RENDER)) {
    printf("failed to load glyph\n");
    return;
  }

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  for (unsigned char c = 0; c < 128; c++) {
    if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
      printf("failed to load glyph\n");
      continue;
    }

    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, face->glyph->bitmap.width,
                 face->glyph->bitmap.rows, 0, GL_RED, GL_UNSIGNED_BYTE,
                 face->glyph->bitmap.buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    Character character = {
        .texture_id = tex,
        .advance = face->glyph->advance.x,
        .size = {face->glyph->bitmap.width, face->glyph->bitmap.rows},
        .bearing = {face->glyph->bitmap_left, face->glyph->bitmap_top},
    };
    characters[c] = character;
  }

  FT_Done_Face(face);
  FT_Done_FreeType(ft);

  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  renderer->text_renderer.vao = vao;

  GLuint vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, 128 * sizeof(TextVertex), NULL,
               GL_DYNAMIC_DRAW);
  renderer->text_renderer.vbo = vbo;

  const GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex_shader, 1, &text_vertex_shader_source, NULL);
  glCompileShader(vertex_shader);

  const GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_shader, 1, &text_fragment_shader_source, NULL);
  glCompileShader(fragment_shader);

  const GLuint shader_program = glCreateProgram();
  // attach and link the vertex and fragment shader
  glAttachShader(shader_program, vertex_shader);
  glAttachShader(shader_program, fragment_shader);
  glLinkProgram(shader_program);
  renderer->text_renderer.shader_program = shader_program;

  glUseProgram(shader_program);

  GLint loc = glGetUniformLocation(shader_program, "projection");
  Mat4Row *projection = ortho(0, state->width, 0, state->height);
  glUniformMatrix4fv(loc, 1, GL_FALSE, (float *)projection);

  free(projection);

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex),
                        (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex),
                        (void *)offsetof(TextVertex, tex));
  glEnableVertexAttribArray(1);
}

void crumble(Piece *piece) {
  int cells_per_block = CELL_SIZE / SAND_SIZE;
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 2; col++) {
      if (piece_shape[piece->piece_type][row][col] == 1) {
        int base_x = (piece->x + col * CELL_SIZE) / SAND_SIZE;
        int base_y = (piece->y + row * CELL_SIZE) / SAND_SIZE;
        for (int sy = 0; sy < cells_per_block; sy++) {
          for (int sx = 0; sx < cells_per_block; sx++) {
            int gx = base_x + sx;
            int gy = base_y + sy;
            if (gx >= 0 && gx < GRID_W && gy >= 0 && gy < GRID_H) {
              cells[gy][gx].cell_type = SAND;
              cells[gy][gx].piece_type = piece->piece_type;
              memcpy(&cells[gy][gx].color, &piece_colors[piece->piece_type],
                     sizeof(piece_colors[0]));
            }
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

  Cell *last_row = cells[row_index];
  int start_col = (int)piece->x / SAND_SIZE;
  int last_col = ((int)piece->x + 2 * CELL_SIZE) / SAND_SIZE;
  for (int i = start_col; i < last_col; i++) {
    if (i >= 0 && i < GRID_W && last_row[i].cell_type == SAND)
      return true;
  }
  return false;
}

void spawn_piece(Renderer *renderer, GLFWwindow *window) {
  State *state = glfwGetWindowUserPointer(window);
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 2; col++) {
      if (piece_shape[state->current_piece.piece_type][row][col] == 1) {
        push_quad(CELL_SIZE, CELL_SIZE,
                  state->current_piece.x + col * CELL_SIZE,
                  state->current_piece.y + row * CELL_SIZE, window,
                  piece_colors[state->current_piece.piece_type]);
      }
    }
  }
  if (collide(&state->current_piece, state)) {
    crumble(&state->current_piece);
    state->current_piece.x = rand() % (state->width - CELL_SIZE * 2);
    state->current_piece.y = 0;
    state->current_piece.piece_type = rand() % PIECE_COUNT;
  }
  double now = glfwGetTime();
  if (state->current_piece.y + 3 * CELL_SIZE < state->height) {
    state->current_piece.y += 1;
  }
}

void update_sand() {
  for (int row = GRID_H - 2; row >= 0; row--) {
    for (int col = 0; col < GRID_W; col++) {
      if (cells[row][col].cell_type == SAND) {
        if (cells[row + 1][col].cell_type == EMPTY) {
          cells[row + 1][col] = cells[row][col];
          cells[row][col].cell_type = EMPTY;
        } else {
          int dir = (rand() % 2) ? 1 : -1;
          if (col + dir >= 0 && col + dir < GRID_W &&
              cells[row + 1][col + dir].cell_type == EMPTY) {
            cells[row + 1][col + dir] = cells[row][col];
            cells[row][col].cell_type = EMPTY;
          } else if (col - dir >= 0 && col - dir < GRID_W &&
                     cells[row + 1][col - dir].cell_type == EMPTY) {
            cells[row + 1][col - dir] = cells[row][col];
            cells[row][col].cell_type = EMPTY;
          }
        }
      }
    }
  }
}

void draw_grid(Renderer *renderer, GLFWwindow *window) {
  for (int row = 0; row < GRID_H; row++) {
    for (int col = 0; col < GRID_W; col++) {
      if (cells[row][col].clearing) {
        bool flash_on = ((int)(glfwGetTime() * 10) % 2 == 0);
        float white[3] = {1.0f, 1.0f, 1.0f};
        push_quad(SAND_SIZE, SAND_SIZE, col * SAND_SIZE, row * SAND_SIZE,
                  window, flash_on ? white : cells[row][col].color);
      } else if (cells[row][col].cell_type == SAND) {
        push_quad(SAND_SIZE, SAND_SIZE, col * SAND_SIZE, row * SAND_SIZE,
                  window, cells[row][col].color);
      }
    }
  }
}

bool flood_fill(int row, int col, int piece_type) {
  bool reached_right = false;
  stack[stack_index++] = (Coord){row, col};

  visited[row][col] = true;

  while (stack_index > 0) {
    Coord c = stack[--stack_index];

    for (int i = 0; i < 4; i++) {
      int nx = c.row + dir[i][0];
      int ny = c.col + dir[i][1];

      if (nx < 0 || nx >= GRID_H || ny < 0 || ny >= GRID_W)
        continue;

      if (visited[nx][ny])
        continue;

      if (cells[nx][ny].piece_type != piece_type ||
          cells[nx][ny].cell_type != SAND || cells[nx][ny].clearing)
        continue;

      visited[nx][ny] = true;

      if (ny == GRID_W - 1)
        reached_right = true;

      stack[stack_index++] = (Coord){.row = nx, .col = ny};
    }
  }

  return reached_right;
}

void clear_line() {
  int left_row[PIECE_COUNT];
  for (int p = 0; p < PIECE_COUNT; p++)
    left_row[p] = -1;

  bool right_has[PIECE_COUNT] = {false};

  for (int row = GRID_H - 1; row >= 0; row--) {
    if (cells[row][0].cell_type == SAND && !cells[row][0].clearing) {
      left_row[cells[row][0].piece_type] = row;
    }
    if (cells[row][GRID_W - 1].cell_type == SAND && !cells[row][0].clearing) {
      right_has[cells[row][GRID_W - 1].piece_type] = true;
    }
  }

  for (int p = 0; p < PIECE_COUNT; p++) {
    if (left_row[p] != -1 && right_has[p]) {
      memset(visited, false, sizeof(visited));
      if (flood_fill(left_row[p], 0, p)) {
        clear_start_time = glfwGetTime();
        for (int row = 0; row < GRID_H; row++) {
          for (int col = 0; col < GRID_W; col++) {
            if (visited[row][col]) {
              cells[row][col].clearing = true;
            }
          }
        }
      }
    }
  }
}

void flush_batch(Renderer *renderer) {
  glBindVertexArray(renderer->vertex_renderer.vao);
  glBindBuffer(GL_ARRAY_BUFFER, renderer->vertex_renderer.vbo);
  glBufferSubData(GL_ARRAY_BUFFER, 0, batch_index * sizeof(Vertex), batch);
  glUseProgram(renderer->vertex_renderer.shader_program);
  glDrawArrays(GL_TRIANGLES, 0, batch_index);
}

void render_text(Renderer *renderer, char *text, float x, float y,
                 float scale) {
  glUseProgram(renderer->text_renderer.shader_program);
  glUniform3f(glGetUniformLocation(renderer->text_renderer.shader_program,
                                   "text_color"),
              0.5f, 0.8f, 0.2f);
  glActiveTexture(GL_TEXTURE0);
  glBindVertexArray(renderer->text_renderer.vao);

  for (int i = 0; text[i] != '\0'; i++) {
    unsigned char c = text[i];
    Character ch = characters[text[i]];

    float xpos = x + ch.bearing[0] * scale;
    float ypos = y - (ch.size[1] - ch.bearing[1]) * scale;

    float w = ch.size[0] * scale;
    float h = ch.size[1] * scale;

    if (i == 0) {
      fprintf(stderr,
              "DEBUG char='%c' tex_id=%u size=(%d,%d) bearing=(%d,%d) "
              "advance=%u xpos=%f ypos=%f w=%f h=%f\n",
              text[i], ch.texture_id, ch.size[0], ch.size[1], ch.bearing[0],
              ch.bearing[1], ch.advance, xpos, ypos, w, h);
    }

    TextVertex vertices[6] = {
        {{xpos, ypos + h}, 0.0f, 0.0f},     {{xpos, ypos}, 0.0f, 1.0f},
        {{xpos + w, ypos}, 1.0f, 1.0f},

        {{xpos, ypos + h}, 0.0f, 0.0f},     {{xpos + w, ypos}, 1.0f, 1.0f},
        {{xpos + w, ypos + h}, 1.0f, 0.0f},
    };

    glBindTexture(GL_TEXTURE_2D, ch.texture_id);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->text_renderer.vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    x += (ch.advance >> 6) * scale;
  }

  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

int main() {
  srand(time(NULL));
  State state = {0};
  Renderer renderer = {0};

  window_init(&renderer, &state);

  vertex_renderer_init(&renderer, &state);
  text_renderer_init(&renderer, &state);

  state.current_piece.x = rand() % (state.width - CELL_SIZE * 2);
  state.current_piece.piece_type = rand() % PIECE_COUNT;

  while (!glfwWindowShouldClose(renderer.window)) {
    glClear(GL_COLOR_BUFFER_BIT);
    batch_index = 0;

    spawn_piece(&renderer, renderer.window);

    update_sand();

    clear_line();
    if (clear_start_time >= 0 &&
        glfwGetTime() - clear_start_time > BLINK_DURATION) {
      for (int row = 0; row < GRID_H; row++) {
        for (int col = 0; col < GRID_W; col++) {
          if (cells[row][col].clearing) {
            cells[row][col].clearing = false;
            cells[row][col].cell_type = EMPTY;
          }
        }
      }
      clear_start_time = -1;
    }
    draw_grid(&renderer, renderer.window);
    flush_batch(&renderer);

    render_text(&renderer, "hello", 0, 0, 4);

    glfwSwapBuffers(renderer.window);
    glfwPollEvents();
  }

  glfwDestroyWindow(renderer.window);
  glfwTerminate();
  exit(EXIT_SUCCESS);
}
