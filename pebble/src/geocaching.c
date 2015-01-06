#include "pebble.h"

// ADDED GPath point definitions for arrow and north marker
static const GPathInfo ARROW_POINTS =
{
  14,
  (GPoint []) {
    {0, -30},
    {10, -20},
    {6, -16},
    {3, -20},
    {3, 10},
    {7, 15},
    {7, 30},
    {0, 22},
    {-7, 30},
    {-7, 15},
    {-3, 10},
    {-3, -20},
    {-6, -16},
    {-10, -20},
  }
};
static const GPathInfo NORTH_POINTS =
{
  3,
  (GPoint []) {
    {0, -40},
    {6, -31},
    {-6, -31},
  }
};

// updated to match new Adroid app
enum GeoKey {
  DISTANCE_KEY = 0x0,
  BEARING_INDEX_KEY = 0x1,
  EXTRAS_KEY = 0x2,
  DT_RATING_KEY = 0x3,
  GC_NAME_KEY = 0x4,
  GC_CODE_KEY = 0x5,
  GC_SIZE_KEY = 0x6,
  AZIMUTH_KEY = 0x7,
  DECLINATION_KEY = 0x8,
};

Window *window;

// ADDED GPath definitions
Layer *arrow_layer;
GPath *arrow;
GPath *north;
GRect arrow_bounds;
GPoint arrow_centre;

int16_t compass_heading = 0;
int16_t north_heading = 0;
int16_t bearing = 0;
uint8_t status;
bool rot_arrow = false;
bool gotdecl = false;
int16_t declination = 0;

TextLayer *text_distance_layer;
TextLayer *text_time_layer;
Layer *line_layer;

static uint8_t data_display = 0;

static AppSync sync;
static uint8_t sync_buffer[150];

void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed);

static void sync_tuple_changed_callback(const uint32_t key,
                                        const Tuple* new_tuple,
                                        const Tuple* old_tuple,
                                        void* context) {

  switch (key) {

    case DISTANCE_KEY:
      text_layer_set_text(text_distance_layer, new_tuple->value->cstring);
      rot_arrow = (strncmp(text_layer_get_text(text_distance_layer), "NO", 2) != 0) ? true : false;
      break;

// NEW
    case AZIMUTH_KEY:
      bearing = new_tuple->value->int16;
      layer_mark_dirty(arrow_layer);
      break;

    case DECLINATION_KEY:
      declination = new_tuple->value->int16;
      gotdecl = true;
      layer_mark_dirty(arrow_layer);
      break;
  }
}

// Draw line between geocaching data and time
void line_layer_update_callback(Layer *layer, GContext* ctx) {
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

// NEW compass handler
void handle_compass(CompassHeadingData heading_data){
/*
  static char buf[64];
  snprintf(buf, sizeof(buf), " %d°", declination);
  text_layer_set_text(text_distance_layer, buf);
*/
  north_heading = TRIGANGLE_TO_DEG(heading_data.magnetic_heading) - declination;
  if (north_heading >= 360) north_heading -= 360;
  if (north_heading < 0) north_heading += 360;
  status = heading_data.compass_status;
  layer_mark_dirty(arrow_layer);
}

// NEW arrow layer update handler
void arrow_layer_update_callback(Layer *path, GContext *ctx) {

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_color(ctx, GColorWhite);

  compass_heading = bearing + north_heading;
  if (compass_heading >= 360) {
    compass_heading = compass_heading - 360;
  }

// Don't rotate the arrow if we have NO GPS or NO BT in the distance box
  if (rot_arrow) {
    gpath_rotate_to(arrow, compass_heading * TRIG_MAX_ANGLE / 360);
  }

// draw outline arrow if pebble compass is invalid, solid if OK
  if (status == 0) {
    gpath_draw_outline(ctx, arrow);
  } else {
    gpath_draw_filled(ctx, arrow);
  }

// draw outline north arrow if pebble compass is fine calibrating, solid if good
  gpath_rotate_to(north, north_heading * TRIG_MAX_ANGLE / 360);
  if (!gotdecl) {
    gpath_draw_outline(ctx, north);
  } else {
    gpath_draw_filled(ctx, north);
  }

/*
// NEW draw a dot at North heading. This has been replaced by an arrow (looks better)

  int north_dot_size = 5;
  int north_length;

  north_length = (arrow_centre.x < arrow_centre.y) ? arrow_centre.x : arrow_centre.y;
  north_length -= north_dot_size / 2;

  int north_x = (int16_t)(sin_lookup(north_heading * TRIG_MAX_ANGLE / 360) * north_length / TRIG_MAX_RATIO) + arrow_centre.x;
  int north_y = (int16_t)(-cos_lookup(north_heading * TRIG_MAX_ANGLE / 360) * north_length / TRIG_MAX_RATIO) + arrow_centre.y;

  graphics_fill_circle(ctx, GPoint(north_x, north_y), north_dot_size);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_circle(ctx, GPoint(north_x, north_y),north_dot_size);
*/
}

void bluetooth_connection_changed(bool connected) {

  if (!connected) {
// deleted bitmap destroy and redraw

// ADDED text instead of bitmap
    text_layer_set_text(text_distance_layer, "NO BT");

    //vibes_short_pulse();

  } else {
    const Tuple *tuple = app_sync_get(&sync, DISTANCE_KEY);
    text_layer_set_text(text_distance_layer, (tuple == NULL) ? "NO GPS" : tuple->value->cstring);
  }
  if (strncmp(text_layer_get_text(text_distance_layer), "NO", 2) != 0) {
    rot_arrow = true;
  } else {
    rot_arrow = false;
  }
}

bool have_additional_data(){
  const Tuple *tuple = app_sync_get(&sync, EXTRAS_KEY);
  return (tuple == NULL || tuple->value->uint8 == 0)? false : true;
}

void up_click_handler(ClickRecognizerRef recognizer, void *context) {

  data_display++;
  data_display %= 5;

  if ( !have_additional_data() ) data_display = 0;

  if(data_display == 0){
    tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
  } else if(data_display == 1){

    tick_timer_service_unsubscribe();
    const Tuple *tuple = app_sync_get(&sync, GC_NAME_KEY);
    const char *gc_data = tuple == NULL ? "" : tuple->value->cstring;
    text_layer_set_text(text_time_layer, *gc_data ? gc_data : "??");

  } else if(data_display == 2){

    const Tuple *tuple = app_sync_get(&sync, GC_CODE_KEY);
    const char *gc_data = tuple == NULL ? "" : tuple->value->cstring;
    text_layer_set_text(text_time_layer, *gc_data ? gc_data : "??");

  } else if(data_display == 3){

    const Tuple *tuple = app_sync_get(&sync, GC_SIZE_KEY);
    const char *gc_data = tuple == NULL ? "" : tuple->value->cstring;
    text_layer_set_text(text_time_layer, *gc_data ? gc_data : "??");

  } else if(data_display == 4){

    const Tuple *tuple = app_sync_get(&sync, DT_RATING_KEY);
    const char *gc_data = tuple == NULL ? "" : tuple->value->cstring;
    text_layer_set_text(text_time_layer, *gc_data ? gc_data : "??");

  }

}

void down_click_handler(ClickRecognizerRef recognizer, void *context) {

  if(data_display == 0) data_display = 5;
  data_display--;

  if ( !have_additional_data() ) data_display = 0;

  if(data_display == 0){
    tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);

  } else if(data_display == 1){

    const Tuple *tuple = app_sync_get(&sync, GC_NAME_KEY);
    const char *gc_data = tuple == NULL ? "" : tuple->value->cstring;
    text_layer_set_text(text_time_layer, *gc_data ? gc_data : "??");

  } else if(data_display == 2){

    const Tuple *tuple = app_sync_get(&sync, GC_CODE_KEY);
    const char *gc_data = tuple == NULL ? "" : tuple->value->cstring;
    text_layer_set_text(text_time_layer, *gc_data ? gc_data : "??");

  } else if(data_display == 3){

    const Tuple *tuple = app_sync_get(&sync, GC_SIZE_KEY);
    const char *gc_data = tuple == NULL ? "" : tuple->value->cstring;
    text_layer_set_text(text_time_layer, *gc_data ? gc_data : "??");

  } else if(data_display == 4){

    tick_timer_service_unsubscribe();

    const Tuple *tuple = app_sync_get(&sync, DT_RATING_KEY);
    const char *gc_data = tuple == NULL ? "" : tuple->value->cstring;
    text_layer_set_text(text_time_layer, *gc_data ? gc_data : "??");

  }

}

void config_buttons_provider(void *context) {
   window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
   window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
 }

void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
// UPDATED to include date
  static char time_text[] = "XXX XX 00:00";

  char *time_format;

// UPDATED to include date
  if (clock_is_24h_style()) {
    time_format = "%b %e %R";
  } else {
    time_format = "%b %e %I:%M";
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

  // Initialize distance layout
  Layer *distance_holder = layer_create(GRect(0, 80, 144, 40));
  layer_add_child(window_layer, distance_holder);

  ResHandle roboto_36 = resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_36);
  text_distance_layer = text_layer_create(GRect(0, 0, 144, 40));
  text_layer_set_text_color(text_distance_layer, GColorWhite);
  text_layer_set_text_alignment(text_distance_layer, GTextAlignmentCenter);
  text_layer_set_background_color(text_distance_layer, GColorClear);
  text_layer_set_font(text_distance_layer, fonts_load_custom_font(roboto_36));
  layer_add_child(distance_holder, text_layer_get_layer(text_distance_layer));

  // Initialize time layout
// Size adjusted so the date and time will fit
  Layer *date_holder = layer_create(GRect(0, 132, 144, 36));
  layer_add_child(window_layer, date_holder);

  line_layer = layer_create(GRect(8, 0, 144-16, 2));
  layer_set_update_proc(line_layer, line_layer_update_callback);
  layer_add_child(date_holder, line_layer);

  ResHandle roboto_22 = resource_get_handle(RESOURCE_ID_FONT_ROBOTO_BOLD_SUBSET_22);
  text_time_layer = text_layer_create(GRect(0, 2, 144, 32));
  text_layer_set_text_color(text_time_layer, GColorWhite);
  text_layer_set_text_alignment(text_time_layer, GTextAlignmentCenter);
  text_layer_set_background_color(text_time_layer, GColorClear);
  text_layer_set_font(text_time_layer, fonts_load_custom_font(roboto_22));
  layer_add_child(date_holder, text_layer_get_layer(text_time_layer));

  // Initialize compass layout
  Layer *compass_holder = layer_create(GRect(0, 0, 144, 79));
  layer_add_child(window_layer, compass_holder);

// deleted bitmap layer create

// NEW definitions for Path layer, adjusted size due to a glitch drawing the North dot
  arrow_layer = layer_create(GRect(0, 0, 144, 79));
  layer_set_update_proc(arrow_layer, arrow_layer_update_callback);
  layer_add_child(compass_holder, arrow_layer);

// centrepoint added for use in rotations
  arrow_bounds = layer_get_frame(arrow_layer);
  arrow_centre = GPoint(arrow_bounds.size.w/2, arrow_bounds.size.h/2);

// Initialize and define the paths
  arrow = gpath_create(&ARROW_POINTS);
  gpath_move_to(arrow, arrow_centre);
  north = gpath_create(&NORTH_POINTS);
  gpath_move_to(north, arrow_centre);

  Tuplet initial_values[] = {
    TupletCString(DISTANCE_KEY, "NO GPS"),
    TupletInteger(BEARING_INDEX_KEY, 0),
    TupletInteger(EXTRAS_KEY, 0),
    TupletCString(DT_RATING_KEY, ""),
    TupletCString(GC_NAME_KEY, ""),
    TupletCString(GC_CODE_KEY, ""),
    TupletCString(GC_SIZE_KEY, ""),
    TupletInteger(AZIMUTH_KEY, 0),
    TupletCString(DECLINATION_KEY, "D"),
  };

  const int inbound_size = 150;
  const int outbound_size = 0;
  app_message_open(inbound_size, outbound_size);

  app_sync_init(&sync, sync_buffer, sizeof(sync_buffer), initial_values,
                ARRAY_LENGTH(initial_values), sync_tuple_changed_callback,
                NULL, NULL);

  // Subscribe to notifications
  bluetooth_connection_service_subscribe(bluetooth_connection_changed);
  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
// compass service added
  compass_service_subscribe(handle_compass);
  compass_service_set_heading_filter(2 * (TRIG_MAX_ANGLE/360));

  window_set_click_config_provider(window, config_buttons_provider);

}

void handle_deinit(void) {
// added unsubscribes
  bluetooth_connection_service_unsubscribe();
  tick_timer_service_unsubscribe();
  compass_service_unsubscribe();

// added layer destroys
  text_layer_destroy(text_time_layer);
  layer_destroy(line_layer);
  text_layer_destroy(text_distance_layer);
  gpath_destroy(arrow);
  layer_destroy(arrow_layer);
  window_destroy(window);

}

int main(void) {
  handle_init();

  app_event_loop();

  handle_deinit();
}