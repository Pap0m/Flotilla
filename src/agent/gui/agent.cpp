#include <print>
#include <raylib.h>
#include <regex>
#include <string>
#include <utility>
#include <zenoh/api/closures.hxx>
#include <zenoh/api/config.hxx>
#include <zenoh/api/query.hxx>
#include <zenoh/api/reply.hxx>
#include <zenoh/api/session.hxx>

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

typedef enum { SCREEN_LOGIN, SCREEN_HOME, SCREEN_SETTINGS } AppScreen;

#define FONT_SIZE 20
#define STRING_SIZE 40

AppScreen current_screen = SCREEN_LOGIN;

typedef struct {
  float login_width;
  float login_height;
  
  // inputs
  bool active_input = true; // true = email, false = password
  char email[STRING_SIZE] = "\0";
  char password[STRING_SIZE] = "\0";

} LoginData;

bool apply_regex_to_string(char* text, std::regex pattern) {
  std::cmatch match;
  if (std::regex_match(text, match, pattern)) {
    return true;
  }
  return false;
}

void draw_login_screen(LoginData &login, float lw, float lh, zenoh::Session& session) {
    // handle keys
    if (IsKeyPressed(KEY_TAB)) {
      login.active_input = !login.active_input;
    }
    std::regex email_regex(R"()");
    std::regex passwd_regex(R"()");

    if (login.email[0] == '\0' ||
        login.password[0] == '\0' ||
        apply_regex_to_string(login.email, email_regex) ||
        apply_regex_to_string(login.password, passwd_regex)) {
      GuiSetStyle(TEXTBOX, BORDER_COLOR_NORMAL, ColorToInt(RED));
    GuiSetStyle(TEXTBOX, TEXT_COLOR_NORMAL, ColorToInt(RED));
    } else {
      GuiSetStyle(TEXTBOX, BORDER_COLOR_NORMAL, ColorToInt(DARKGRAY));
    GuiSetStyle(TEXTBOX, TEXT_COLOR_NORMAL, ColorToInt(BLACK));
    }
  
    // Layout vars
    float padding = 40.0f;
    float entry_width = login.login_width - (padding * 2.0f);
    float entry_height = 35.0f;
    float spacing = 20.0f;

    Rectangle login_rect = {
      (lw - login.login_width) / 2.0f,
      (lh - login.login_height) / 2.0f,
      login.login_width,
      login.login_height
    };
    GuiGroupBox(login_rect, "Login");

    // Email Section
    float current_y = login_rect.y + 40.f;
    GuiLabel((Rectangle){ login_rect.x + padding, current_y, entry_width, 20 }, "Email");

    current_y += 25.0f;
    GuiTextBox((Rectangle){ login_rect.x + padding, current_y, entry_width, entry_height }, login.email, STRING_SIZE, (login.active_input == true));
    
    // Password Section
    current_y += entry_height + spacing;
    GuiLabel((Rectangle){ login_rect.x + padding, current_y, entry_width, 20 }, "Password");

    current_y += 25.0f;
    GuiTextBox((Rectangle){ login_rect.x + padding, current_y, entry_width, entry_height }, login.password, STRING_SIZE, (login.active_input == false));

    // Button Section
    current_y += entry_height + spacing + 20;
    if (GuiButton((Rectangle){ login_rect.x + padding, current_y, entry_width, 45 }, "LOG IN")) {
      // prepare credentials string
      std::string credentials = std::string(login.email) + ":" + std::string(login.password);
      // get the response form the service
      session.get("agent/ipc/login", credentials, [](const zenoh::Reply& reply) {
                    if (reply.is_ok()) {
                      const auto& sample = reply.get_ok();
                      std::string status = sample.get_payload().as_string();
                      std::println("Login status: {}", status);

                      if (status == "OK") {
                        current_screen = SCREEN_HOME;
                      }
                    } else {
                      std::println("Query error: {}", reply.get_err().get_payload().as_string().c_str());
                    }
                  }, []() {}
                  );
    }
    

    // resert colors
    GuiSetStyle(TEXTBOX, BORDER_COLOR_NORMAL, ColorToInt(DARKGRAY));
    GuiSetStyle(TEXTBOX, TEXT_COLOR_NORMAL, ColorToInt(BLACK));
}

int main() {
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI);

  InitWindow(800, 600, "Agent");

  MaximizeWindow();

  SetTargetFPS(60);
  SetExitKey(-1);

  // set Width and Height
  Vector2 scale = GetWindowScaleDPI();
  float lw = (float)GetRenderWidth();
  float lh = (float)GetRenderHeight();
  
  SetWindowMinSize((int)((lw * scale.x) / 2.0f), (int)((lh * scale.y) / 2.0f));

  // config text style
  GuiSetStyle(DEFAULT, TEXT_SIZE, FONT_SIZE);

  // initialize login
  LoginData login = { 0 };
  login.login_width = 450;
  login.login_height = 350;
  login.active_input = true;
  login.email[0] = '\0';
  login.password[0] = '\0';

  // zenoh
  zenoh::Config config = zenoh::Config::create_default();
  // config.insert_json5("connect/endpoints", "[\"unix:/tmp/agentd.sock\"]");
  config.insert_json5("connect/endpoints", "[\"tcp/127.0.0.1:7447\"]");
  std::optional<zenoh::Session> session;
  try {
    session = zenoh::Session::open(std::move(config));
    std::println("Connected to agentd.");
  } catch (const zenoh::ZException& e) {
    std::println(stderr, "Could not connect to agentd: {}. Is the backend running?", e.what());
    // You can decide to exit or run in "offline mode"
    return 1; 
  }
  
  while (!WindowShouldClose()) {
    // update vars
    scale = GetWindowScaleDPI();
    lw = (float)GetRenderWidth() / scale.x;
    lh = (float)GetRenderHeight() / scale.y;

   
    BeginDrawing();
    ClearBackground(RAYWHITE);

    switch (current_screen) {
      case SCREEN_LOGIN:
        draw_login_screen(login, lw, lh, *session);
        break;

      default:
        std::unreachable();
    }

    EndDrawing();
  }

  CloseWindow();

  return 0;
}
