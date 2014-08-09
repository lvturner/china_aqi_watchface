#include <pebble.h>

static Window *window;
static TextLayer *aqi_value_layer;
static TextLayer *time_layer;
static TextLayer *temp_layer;

static AppSync sync;
static uint8_t sync_buffer[64];

enum {
  CITY = 0x1,
  AQI = 0x2,
  TEMPERATURE = 0x4
};

void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
  switch (key) {
    case AQI:
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "Persisting %u for AQI", new_tuple->value->int);
        if((int)new_tuple->value->uint32 != -1) {
          persist_write_int(AQI, (int)new_tuple->value->uint32);
          static char aqi_text[5];
          snprintf(aqi_text, sizeof(aqi_text), "%u", (int)new_tuple->value->uint32);
          text_layer_set_text(aqi_value_layer, aqi_text);
        }
      break;
    case TEMPERATURE:
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Persisting %u for TEMPERATURE", (int)new_tuple->value->uint32);
        if((int)new_tuple->value->uint32 != -1) {
          persist_write_int(TEMPERATURE, (int)new_tuple->value->uint32);
          static char temperature_text[6];
          snprintf(temperature_text, sizeof(temperature_text), "%u\u00B0C", (int)new_tuple->value->uint32);
          text_layer_set_text(temp_layer, temperature_text);
        }
      break;
  }
}

// http://stackoverflow.com/questions/21150193/logging-enums-on-the-pebble-watch/21172222#21172222
char *translate_error(AppMessageResult result) {
  switch (result) {
    case APP_MSG_OK: return "APP_MSG_OK";
    case APP_MSG_SEND_TIMEOUT: return "APP_MSG_SEND_TIMEOUT";
    case APP_MSG_SEND_REJECTED: return "APP_MSG_SEND_REJECTED";
    case APP_MSG_NOT_CONNECTED: return "APP_MSG_NOT_CONNECTED";
    case APP_MSG_APP_NOT_RUNNING: return "APP_MSG_APP_NOT_RUNNING";
    case APP_MSG_INVALID_ARGS: return "APP_MSG_INVALID_ARGS";
    case APP_MSG_BUSY: return "APP_MSG_BUSY";
    case APP_MSG_BUFFER_OVERFLOW: return "APP_MSG_BUFFER_OVERFLOW";
    case APP_MSG_ALREADY_RELEASED: return "APP_MSG_ALREADY_RELEASED";
    case APP_MSG_CALLBACK_ALREADY_REGISTERED: return "APP_MSG_CALLBACK_ALREADY_REGISTERED";
    case APP_MSG_CALLBACK_NOT_REGISTERED: return "APP_MSG_CALLBACK_NOT_REGISTERED";
    case APP_MSG_OUT_OF_MEMORY: return "APP_MSG_OUT_OF_MEMORY";
    case APP_MSG_CLOSED: return "APP_MSG_CLOSED";
    case APP_MSG_INTERNAL_ERROR: return "APP_MSG_INTERNAL_ERROR";
    default: return "UNKNOWN ERROR";
  }
}

void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "... Sync Error: %s", translate_error(app_message_error));
}

static void ask_for_update() {
  Tuplet value = TupletInteger(1, 1);

  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  if (iter == NULL) {
    return;
  }

  dict_write_tuplet(iter, &value);
  dict_write_end(iter);

  app_message_outbox_send();
}

static void handle_tick(struct tm* tick_time, TimeUnits units_changed) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Handled minute");
  static char time_text[] = "00:00";
  strftime(time_text, sizeof(time_text), "%I:%M", tick_time);
  text_layer_set_text(time_layer, time_text);
  if(units_changed & HOUR_UNIT) {
    ask_for_update();
  }
}

static void init_clock(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  time_layer = text_layer_create(GRect(0, 30, bounds.size.w, bounds.size.h-100));
  text_layer_set_text_alignment(time_layer, GTextAlignmentCenter);
  text_layer_set_text_color(time_layer, GColorWhite);
  text_layer_set_background_color(time_layer, GColorClear);
  text_layer_set_font(time_layer, fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));

  time_t now = time(NULL);
  struct tm *current_time = localtime(&now);
  handle_tick(current_time, MINUTE_UNIT);
  tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(time_layer));

  temp_layer = text_layer_create(GRect(0, 130, (bounds.size.w/2), bounds.size.h-130));
  text_layer_set_text_color(temp_layer, GColorBlack);
  text_layer_set_text_alignment(temp_layer, GTextAlignmentCenter);
  text_layer_set_background_color(temp_layer, GColorWhite);
  text_layer_set_overflow_mode(temp_layer, GTextOverflowModeFill);
  text_layer_set_font(temp_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(temp_layer));

  aqi_value_layer = text_layer_create(GRect(bounds.size.w/2, 130, (bounds.size.w/2), bounds.size.h-130));
  text_layer_set_text_color(aqi_value_layer, GColorBlack);
  text_layer_set_text_alignment(aqi_value_layer, GTextAlignmentCenter);
  text_layer_set_background_color(aqi_value_layer, GColorWhite);
  text_layer_set_overflow_mode(aqi_value_layer, GTextOverflowModeFill);
  text_layer_set_font(aqi_value_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(aqi_value_layer));

  int aqi = persist_exists(AQI) ? persist_read_int(AQI) : -1;
  int temperature = persist_exists(TEMPERATURE) ? persist_read_int(TEMPERATURE) : -1;
  Tuplet initial_values[] = {
     TupletInteger(AQI, aqi),
     TupletInteger(TEMPERATURE, temperature)
  };

  app_sync_init(&sync, sync_buffer, sizeof(sync_buffer), initial_values, ARRAY_LENGTH(initial_values),
      sync_tuple_changed_callback, sync_error_callback, NULL);
}


static void window_load(Window *window) {
  init_clock(window);
}

static void window_unload(Window *window) {
  app_sync_deinit(&sync);
  text_layer_destroy(aqi_value_layer);
  text_layer_destroy(time_layer);
}

static void init(void) {
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  app_message_open(64, 64);

  const bool animated = true;
  window_stack_push(window, animated);
  window_set_background_color(window, GColorBlack);
}

static void deinit(void) {
  window_destroy(window);
}

int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}
