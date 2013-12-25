#include "pebble.h"

static const uint32_t COMPASS_ICONS[] = {
  RESOURCE_ID_DIRECTION_0,
  RESOURCE_ID_DIRECTION_1,
  RESOURCE_ID_DIRECTION_2,
  RESOURCE_ID_DIRECTION_3,
  RESOURCE_ID_DIRECTION_4,
  RESOURCE_ID_DIRECTION_5,
  RESOURCE_ID_DIRECTION_6,
  RESOURCE_ID_DIRECTION_7,
  RESOURCE_ID_DIRECTION_8,
  RESOURCE_ID_DIRECTION_9,
  RESOURCE_ID_DIRECTION_10,
  RESOURCE_ID_DIRECTION_11
};

enum GeoKey {
  DISTANCE_KEY = 0x0,
  AZIMUT_INDEX_KEY = 0x1,
};

Window *window;

BitmapLayer *image_layer;
GBitmap *image_bitmap = NULL;

TextLayer *text_distance_layer;
TextLayer *text_time_layer;
Layer *line_layer;

static AppSync sync;
static uint8_t sync_buffer[64];

static void sync_tuple_changed_callback(const uint32_t key,
                                        const Tuple* new_tuple,
                                        const Tuple* old_tuple,
                                        void* context) {

  switch (key) {

    case DISTANCE_KEY:
      text_layer_set_text(text_distance_layer, new_tuple->value->cstring);
      break;

    case AZIMUT_INDEX_KEY:
      if (image_bitmap) {
        gbitmap_destroy(image_bitmap);
      }

      image_bitmap = gbitmap_create_with_resource(
          COMPASS_ICONS[new_tuple->value->uint8]);
      bitmap_layer_set_bitmap(image_layer, image_bitmap);
      break;
  }
}

// Draw line between geocaching data and time
void line_layer_update_callback(Layer *layer, GContext* ctx) {
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

void bluetooth_connection_changed(bool connected) {

  if (!connected) {
    
    if (image_bitmap) {
      gbitmap_destroy(image_bitmap);
    }

    image_bitmap = gbitmap_create_with_resource(RESOURCE_ID_NO_BT);
    bitmap_layer_set_bitmap(image_layer, image_bitmap);
    
    //vibes_short_pulse();
  }
  
}

void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  static char time_text[] = "00:00";

  char *time_format;

  if (clock_is_24h_style()) {
    time_format = "%R";
  } else {
    time_format = "%I:%M";
  }

  strftime(time_text, sizeof(time_text), time_format, tick_time);

  if (!clock_is_24h_style() && (time_text[0] == '0')) {
    text_layer_set_text(text_time_layer, time_text + 1);
  } else {
    text_layer_set_text(text_time_layer, time_text);
  }
}

void handle_init(void) {
  window = window_create();
  
  window_set_background_color(window, GColorBlack);

  window_set_fullscreen(window, true);
  window_stack_push(window, true);

  Layer *window_layer = window_get_root_layer(window);

  // Initialize compass layout
  Layer *compass_holder = layer_create(GRect(0, 0, 144, 79));
  layer_add_child(window_layer, compass_holder);

  image_layer = bitmap_layer_create(GRect(42, 15, 60, 60));
  layer_add_child(compass_holder, bitmap_layer_get_layer(image_layer));

  // Initialize distance layout
  Layer *distance_holder = layer_create(GRect(0, 80, 144, 40));
  layer_add_child(window_layer, distance_holder);

  ResHandle roboto_36 = resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_36);

  text_distance_layer = text_layer_create(GRect(8, 0, 144-8, 40));
  text_layer_set_text_color(text_distance_layer, GColorWhite);
  text_layer_set_text_alignment(text_distance_layer, GTextAlignmentCenter);
  text_layer_set_background_color(text_distance_layer, GColorClear);
  text_layer_set_font(text_distance_layer, fonts_load_custom_font(roboto_36));
  layer_add_child(distance_holder, text_layer_get_layer(text_distance_layer));


  // Initialize time layout
  Layer *date_holder = layer_create(GRect(0, 132, 144, 36));
  layer_add_child(window_layer, date_holder);

  line_layer = layer_create(GRect(8, 0, 144-16, 2));
  layer_set_update_proc(line_layer, line_layer_update_callback);
  layer_add_child(date_holder, line_layer);

  ResHandle roboto_32 = resource_get_handle(RESOURCE_ID_FONT_ROBOTO_BOLD_SUBSET_32);
  text_time_layer = text_layer_create(GRect(32, -2, 144-32, 34));
  text_layer_set_text_color(text_time_layer, GColorWhite);
  text_layer_set_background_color(text_time_layer, GColorClear);
  text_layer_set_font(text_time_layer, fonts_load_custom_font(roboto_32));
  layer_add_child(date_holder, text_layer_get_layer(text_time_layer));

  // Setup messaging
  const int inbound_size = 64;
  const int outbound_size = 64;
  app_message_open(inbound_size, outbound_size);

  Tuplet initial_values[] = {
    TupletCString(DISTANCE_KEY, "Starting.."),
    TupletInteger(AZIMUT_INDEX_KEY, 0),
  };

  app_sync_init(&sync, sync_buffer, sizeof(sync_buffer), initial_values,
                ARRAY_LENGTH(initial_values), sync_tuple_changed_callback,
                NULL, NULL);

  // Subscribe to notifications
  bluetooth_connection_service_subscribe(bluetooth_connection_changed);
  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);

}

void handle_deinit(void) {
  bluetooth_connection_service_unsubscribe();
  tick_timer_service_unsubscribe();
}

int main(void) {
  handle_init();

  app_event_loop();

  handle_deinit();
}
