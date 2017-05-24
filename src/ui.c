#include <stdlib.h>
#include <string.h>

#include "hal.h"

#include "chprintf.h"
#include "gfx.h"
#include "oled.h"
#include "ui.h"
#include "fpga_config.h"

unsigned int time_map[TIME_STATES] = {2, 5, 10, 30, 3600}; // time options, in seconds

virtual_timer_t ui_vt;
event_source_t ui_timer_event;

virtual_timer_t seconds_vt;

ui_monitor  uimon;
ui_config   uicfg;
ui_battery  uibat;
ui_input    uiinput;
ui_graph    uigraph;

#define STATE_MON 0
#define STATE_CFG 1
#define STATE_GRAPH 2

static uint8_t ui_state = STATE_GRAPH;
static uint8_t state_line = 1;
#define CONFIG_LINES 3
static uint8_t state2_line = 1;
#define CONFIG2_LINES 2

#define GRAPH_HEIGHT 14
#define LEGEND_WIDTH 32
#define GRAPH_WIDTH 96

void buzzer(int state);

static int log_has_event(uint16_t *events, unsigned int search_window_ms) {
  int i;
  int num_log_entries;
  int has_event = 0;

  num_log_entries = search_window_ms / uicfg.log_interval;
  if( num_log_entries == 0 )
    num_log_entries = 1;
  if( num_log_entries > LOGLEN )
    num_log_entries = LOGLEN;

  for( i = 0; i < num_log_entries; i++ ) {
    if( events[( ((int)uigraph.log_index) - (1 + i) + LOGLEN) % LOGLEN] != 0 )
      has_event = 1;
  }

  return has_event;
}

void updateUI(void) {

  //  chMtxLock(&uigraph.log_mutex);
  
  if( ST2MS(chVTTimeElapsedSinceX( uigraph.last_log_time )) < uicfg.log_interval )
    return;

  uigraph.last_log_time = chVTGetSystemTime();

  uigraph.log_index = (uigraph.log_index + 1) % LOGLEN;

  uigraph.gps_events[uigraph.log_index] = 0;
  uigraph.bt_events[uigraph.log_index] = 0;
  uigraph.wifi_events[uigraph.log_index] = 0;
  uigraph.cell_events[uigraph.log_index] = 0;
  
  //  chMtxUnlock(&uigraph.log_mutex);
}
 
void uiHandler(eventid_t id) {
  (void) id;

  coord_t width, fontheight;
  coord_t fontlinespace;
  coord_t cur_line = 0;
  font_t font;
  font_t font_small;
  char str[32];
  char substr[8];
  int i;
  uint16_t maxval;
  int dark = 0;

  chMtxLock(&uigraph.log_mutex);  ////// lock uigraph
    
  updateUI();

  // rising edge detect on uiinput.a
  if( uiinput.a == 1 ) {
    if( ui_state == STATE_MON )
      ui_state = STATE_CFG;
    else if( ui_state == STATE_CFG )
      ui_state = STATE_GRAPH;
    else
      ui_state = STATE_MON; 

    uiinput.a = 0;
  }
  
  oledGfxStart();
  width = gdispGetWidth();
  
  if( ui_state == STATE_MON ) {
#if 0    
    gdispClear(Black);

    font = gdispOpenFont("fixed_5x8"); // can fit 32 chars wide
    fontheight = gdispGetFontMetric(font, fontHeight);
    chsnprintf(str, sizeof(str), " %4dmV              %3d%%", uibat.batt_mv, uibat.batt_soc / 10 );
    gdispDrawStringBox(0, 0, width, fontheight,
		       str, font, White, justifyLeft);
    cur_line += fontheight;
    gdispCloseFont(font);
    
    font = gdispOpenFont("fixed_7x14"); // can fit 18 chars wide
    fontheight = gdispGetFontMetric(font, fontHeight);
    fontlinespace = fontheight - 4; // line space a bit closer than the total height of font reported

    if( uimon.status == UI_DARK )
      chsnprintf(str, sizeof(str), "Status: DARK");
    else
      chsnprintf(str, sizeof(str), "Status: LIVE");
    gdispDrawStringBox(0, cur_line, width, cur_line + fontheight,
		       str, font, White, justifyLeft);
    cur_line += fontlinespace;

    chsnprintf(str, sizeof(str), "Test state: %s", "BAD" ); // eventually update to pass/fail once we get there...
    gdispDrawStringBox(0, cur_line, width, cur_line + fontheight,
		       str, font, White, justifyLeft);
    cur_line += fontlinespace;

    chsnprintf(str, sizeof(str), "gps: %s cell: %s", uimon.gps_ok == UI_OK ? " OK" : "BAD",
	       uimon.cell_ok == UI_OK ? " OK " : "BAD" );
    gdispDrawStringBox(0, cur_line, width, cur_line + fontheight,
		       str, font, White, justifyLeft);
    cur_line += fontlinespace;
    
    chsnprintf(str, sizeof(str), "bt : %s wifi: %s", uimon.bt_ok == UI_OK ? " OK" : "BAD",
	       uimon.wifi_ok == UI_OK ? " OK " : "BAD" );
    gdispDrawStringBox(0, cur_line, width, cur_line + fontheight,
		       str, font, White, justifyLeft);
#else
    if( uiinput.down == 1 ) {
      state2_line++;
      if( state2_line == (CONFIG2_LINES + 1) )
	state2_line = 1;
      uiinput.down = 0;
    }
    if( uiinput.up == 1 ) {
      state2_line--;
      if( state2_line == 0 )
	state2_line = CONFIG2_LINES;
      uiinput.up = 0;
    }
    
    gdispClear(Black);

    font = gdispOpenFont("fixed_5x8"); // can fit 32 chars wide
    fontheight = gdispGetFontMetric(font, fontHeight);
    chsnprintf(str, sizeof(str), " %4dmV              %3d%%", uibat.batt_mv, uibat.batt_soc / 10 );
    gdispDrawStringBox(0, 0, width, fontheight,
		       str, font, White, justifyLeft);
    cur_line += fontheight;
    gdispCloseFont(font);
    
    font = gdispOpenFont("fixed_7x14"); // can fit 18 chars wide
    fontheight = gdispGetFontMetric(font, fontHeight);
    fontlinespace = fontheight; // line space a bit closer than the total height of font reported

    switch(uimon.status) {
    case UI_DARK:
      chsnprintf(substr, sizeof(substr), "DARK");
      break;
    case UI_LIVE:
    case UI_LIVE_DEBOUNCE:
      chsnprintf(substr, sizeof(substr), "LIVE");
      break;
    case UI_ALRM:
      chsnprintf(substr, sizeof(substr), "ALRM");
      break;
    case UI_NOTE:
      chsnprintf(substr, sizeof(substr), "NOTE");
      break;
    default:
      chsnprintf(substr, sizeof(substr), "????");
    }
    chsnprintf(str, sizeof(str), "%s for %ds", substr, uimon.state_elapsed_time );
    gdispFillStringBox(0, cur_line, width, fontlinespace, str, font, Black, White, justifyCenter);
    cur_line += fontlinespace;

    if( state2_line == 1) {
      gdispDrawString(0, cur_line, ">", font, White);
      if( uiinput.right == 1 ) {
	uicfg.alarmtime = (uicfg.alarmtime + 1) % TIME_STATES;
	uiinput.right = 0;
      }
    }
    chsnprintf(str, sizeof(str), " Alarm time: %ds", time_map[uicfg.alarmtime]);
    gdispDrawString(0, cur_line, str, font, White);
    cur_line += fontlinespace;

    if( state2_line == 2) {
      gdispDrawString(0, cur_line, ">", font, White);
      if( uiinput.right == 1 ) {
	uicfg.darkdelay = (uicfg.darkdelay + 1) % (TIME_STATES - 1);  // don't allow the longer setting
	uiinput.right = 0;
      }
    }
    chsnprintf(str, sizeof(str), " Dark delay: %ds", time_map[uicfg.darkdelay]);
    gdispDrawString(0, cur_line, str, font, White);
    cur_line += fontlinespace;
    
    gdispCloseFont(font);
    
#endif
    
  } else if( ui_state == STATE_CFG ) {
    if( uiinput.down == 1 ) {
      state_line++;
      if( state_line == (CONFIG_LINES + 1) )
	state_line = 1;
      uiinput.down = 0;
    }
    if( uiinput.up == 1 ) {
      state_line--;
      if( state_line == 0 )
	state_line = CONFIG_LINES;
      uiinput.up = 0;
    }
    
    gdispClear(Black);

    font = gdispOpenFont("fixed_5x8"); // can fit 32 chars wide
    fontheight = gdispGetFontMetric(font, fontHeight);
    chsnprintf(str, sizeof(str), " %4dmV              %3d%%", uibat.batt_mv, uibat.batt_soc / 10 );
    gdispDrawStringBox(0, 0, width, fontheight,
		       str, font, White, justifyLeft);
    cur_line += fontheight;
    gdispCloseFont(font);
    
    font = gdispOpenFont("fixed_7x14"); // can fit 18 chars wide
    fontheight = gdispGetFontMetric(font, fontHeight);
    fontlinespace = fontheight; // line space a bit closer than the total height of font reported
    
    switch(uimon.status) {
    case UI_DARK:
      chsnprintf(substr, sizeof(substr), "DARK");
      break;
    case UI_LIVE:
    case UI_LIVE_DEBOUNCE:
      chsnprintf(substr, sizeof(substr), "LIVE");
      break;
    case UI_ALRM:
      chsnprintf(substr, sizeof(substr), "ALRM");
      break;
    case UI_NOTE:
      chsnprintf(substr, sizeof(substr), "NOTE");
      break;
    default:
      chsnprintf(substr, sizeof(substr), "????");
    }
    chsnprintf(str, sizeof(str), "%s for %ds", substr, uimon.state_elapsed_time );
    gdispFillStringBox(0, cur_line, width, fontlinespace, str, font, Black, White, justifyCenter);
    cur_line += fontlinespace;
    
    // put cfg ui here
    if( state_line == 1) {
      gdispDrawString(0, cur_line, ">", font, White);
      if( uiinput.right == 1 ) {
	touch_debounce = MS2ST(2000);
	if(uicfg.simsel == 1) {
	  // action here
	  palSetPad(IOPORT1, 19); // sim OUT
	  palSetPad(IOPORT3, 9); // set SIM_SEL, selecting sim2
	  chThdSleepMilliseconds(1000);
	  palClearPad(IOPORT1, 19); // sim IN
	  uicfg.simsel = 2;
	} else { 
	  // action here
	  palSetPad(IOPORT1, 19); // sim OUT
	  palClearPad(IOPORT3, 9); // clear SIM_SEL, selecting sim1
	  chThdSleepMilliseconds(1000);
	  palClearPad(IOPORT1, 19); // sim IN
	  uicfg.simsel = 1;
	}
	uiinput.right = 0;
      }
    }
    gdispDrawString(0, cur_line, " SIM: ", font, White);
    if( uicfg.simsel == 1 ) {
      gdispFillString(gdispGetStringWidth(" SIM: ", font) , cur_line, "SIM1", font, Black, White);
      gdispFillString(gdispGetStringWidth(" SIM: SIM1 ", font) , cur_line, "SIM2", font, White, Black);
    } else {
      gdispFillString(gdispGetStringWidth(" SIM: ", font) , cur_line, "SIM1", font, White, Black);
      gdispFillString(gdispGetStringWidth(" SIM: SIM1 ", font) , cur_line, "SIM2", font, Black, White);
    }
    
    cur_line += fontlinespace;

    if( state_line == 2) {
      gdispDrawString(0, cur_line, ">", font, White);
      if( uiinput.right == 1 ) {
	if(uicfg.alarmon == 1) {
	  // action here
	  uicfg.alarmon = 0;
	} else {
	  // action here
	  uicfg.alarmon = 1;
	}
	uiinput.right = 0;
      }
    }
    gdispDrawString(0, cur_line, " Alarm: ", font, White);
    if( uicfg.alarmon == 1 ) {
      gdispFillString(gdispGetStringWidth(" Alarm: ", font) , cur_line, "ON", font, Black, White);
      gdispFillString(gdispGetStringWidth(" Alarm: ON ", font) , cur_line, "OFF", font, White, Black);
    } else {
      gdispFillString(gdispGetStringWidth(" Alarm: ", font) , cur_line, "ON", font, White, Black);
      gdispFillString(gdispGetStringWidth(" Alarm: ON ", font) , cur_line, "OFF", font, Black, White);
    }
    cur_line += fontlinespace;

    if( state_line == 3) {
      gdispDrawString(0, cur_line, ">", font, White);
      if( uiinput.right == 1 ) {
	if(uicfg.notifyon == 1) {
	  uicfg.notifyon = 0;
	} else {
	  // action here
	  uicfg.notifyon = 1;
	}
	uiinput.right = 0;
      }
    }
    gdispDrawString(0, cur_line, " Notify: ", font, White);
    if( uicfg.notifyon == 1 ) {
      gdispFillString(gdispGetStringWidth(" Notify: ", font) , cur_line, "ON", font, Black, White);
      gdispFillString(gdispGetStringWidth(" Notify: ON ", font) , cur_line, "OFF", font, White, Black);
    } else {
      gdispFillString(gdispGetStringWidth(" Notify: ", font) , cur_line, "ON", font, White, Black);
      gdispFillString(gdispGetStringWidth(" Notify: ON ", font) , cur_line, "OFF", font, Black, White);
    }
    cur_line += fontlinespace;

    gdispCloseFont(font);
  } else if( ui_state == STATE_GRAPH ) {
    gdispClear(Black);

    font_small = gdispOpenFont("fixed_5x8"); // can fit 32 chars wide
    fontheight = gdispGetFontMetric(font_small, fontHeight);
    switch(uimon.status) {
    case UI_DARK:
      chsnprintf(substr, sizeof(substr), "DARK");
      break;
    case UI_LIVE:
    case UI_LIVE_DEBOUNCE:
      chsnprintf(substr, sizeof(substr), "LIVE");
      break;
    case UI_ALRM:
      chsnprintf(substr, sizeof(substr), "ALRM");
      break;
    case UI_NOTE:
      chsnprintf(substr, sizeof(substr), "NOTE");
      break;
    default:
      chsnprintf(substr, sizeof(substr), "????");
    }

    chsnprintf(str, sizeof(str), " %4dmV %s  %5ds %3d%%", uibat.batt_mv, substr,
	       uimon.state_elapsed_time, uibat.batt_soc / 10 );
    gdispDrawStringBox(0, 0, width, fontheight,
		       str, font_small, White, justifyLeft);
    cur_line += fontheight;
    
    font = gdispOpenFont("fixed_7x14"); // can fit 18 chars wide
    fontheight = gdispGetFontMetric(font, fontHeight);
    fontlinespace = fontheight - 4; // line space a bit closer than the total height of font reported

    chsnprintf(str, sizeof(str), "cell" );
    gdispDrawStringBox(0, cur_line, width, GRAPH_HEIGHT,
		       str, font, White, justifyLeft);
    for( i = 0, maxval = 0; i < LOGLEN; i++ ) {
      if( uigraph.cell_events[i] > maxval )
	maxval = uigraph.cell_events[i];
    }
    if( maxval != 0 ) {
      for( i = 0; i < LOGLEN - 1; i++ ) {
	gdispDrawLine(LEGEND_WIDTH + i,
         cur_line + GRAPH_HEIGHT - (uigraph.cell_events[(i+uigraph.log_index+1) % LOGLEN] * (GRAPH_HEIGHT - 2)) / maxval - 1,
         LEGEND_WIDTH + i + 1,
         cur_line + GRAPH_HEIGHT - (uigraph.cell_events[((i+uigraph.log_index+1) + 1) % LOGLEN] * (GRAPH_HEIGHT - 2)) / maxval - 1,
         White);
      }
    } else {
      gdispDrawStringBox(LEGEND_WIDTH, cur_line, width - LEGEND_WIDTH, GRAPH_HEIGHT, "(dark)", font_small, White, justifyCenter);
    }
    cur_line += GRAPH_HEIGHT;

    chsnprintf(str, sizeof(str), "gps " );
    gdispDrawStringBox(0, cur_line, width, GRAPH_HEIGHT,
		       str, font, White, justifyLeft);
    for( i = 0, maxval = 0; i < LOGLEN; i++ ) {
      if( uigraph.gps_events[i] > maxval )
	maxval = uigraph.gps_events[i];
    }
    if( maxval != 0 ) {
      for( i = 0; i < LOGLEN - 1; i++ ) {
	gdispDrawLine(LEGEND_WIDTH + i,
         cur_line + GRAPH_HEIGHT - (uigraph.gps_events[(i+uigraph.log_index+1) % LOGLEN] * (GRAPH_HEIGHT - 2)) / maxval - 1,
         LEGEND_WIDTH + i + 1,
         cur_line + GRAPH_HEIGHT - (uigraph.gps_events[((i+uigraph.log_index+1) + 1) % LOGLEN] * (GRAPH_HEIGHT - 2)) / maxval - 1,
         White);
      }
    } else {
      gdispDrawStringBox(LEGEND_WIDTH, cur_line, width - LEGEND_WIDTH, GRAPH_HEIGHT, "(dark)", font_small, White, justifyCenter);
    }       
    cur_line += GRAPH_HEIGHT;

    chsnprintf(str, sizeof(str), "wifi" );
    gdispDrawStringBox(0, cur_line, width, GRAPH_HEIGHT,
		       str, font, White, justifyLeft);
    for( i = 0, maxval = 0; i < LOGLEN; i++ ) {
      if( uigraph.wifi_events[i] > maxval )
	maxval = uigraph.wifi_events[i];
    }
    if( maxval != 0 ) {
      for( i = 0; i < LOGLEN - 1; i++ ) {
	gdispDrawLine(LEGEND_WIDTH + i,
         cur_line + GRAPH_HEIGHT - (uigraph.wifi_events[(i+uigraph.log_index+1) % LOGLEN] * (GRAPH_HEIGHT - 2)) / maxval - 1,
         LEGEND_WIDTH + i + 1,
         cur_line + GRAPH_HEIGHT - (uigraph.wifi_events[((i+uigraph.log_index+1) + 1) % LOGLEN] * (GRAPH_HEIGHT - 2)) / maxval - 1,
         White);
      }
    } else {
      gdispDrawStringBox(LEGEND_WIDTH, cur_line, width - LEGEND_WIDTH, GRAPH_HEIGHT, "(dark)", font_small, White, justifyCenter);
    }
    cur_line += GRAPH_HEIGHT;

    chsnprintf(str, sizeof(str), "bt  " );
    gdispDrawStringBox(0, cur_line, width, GRAPH_HEIGHT,
		       str, font, White, justifyLeft);
    for( i = 0, maxval = 0; i < LOGLEN; i++ ) {
      if( uigraph.bt_events[i] > maxval )
	maxval = uigraph.bt_events[i];
    }
    if( maxval != 0 ) {
      for( i = 0; i < LOGLEN - 1; i++ ) {
	gdispDrawLine(LEGEND_WIDTH + i,
         cur_line + GRAPH_HEIGHT - (uigraph.bt_events[(i+uigraph.log_index+1) % LOGLEN] * (GRAPH_HEIGHT - 2)) / maxval - 1,
         LEGEND_WIDTH + i + 1,
         cur_line + GRAPH_HEIGHT - (uigraph.bt_events[((i+uigraph.log_index+1) + 1) % LOGLEN] * (GRAPH_HEIGHT - 2)) / maxval - 1,
         White);
      }
    } else {
      gdispDrawStringBox(LEGEND_WIDTH, cur_line, width - LEGEND_WIDTH, GRAPH_HEIGHT, "(dark)", font_small, White, justifyCenter);
    }
    cur_line += GRAPH_HEIGHT;

    gdispCloseFont(font);
    gdispCloseFont(font_small);
  }

  dark = 1;
  if( log_has_event(uigraph.gps_events, time_map[uicfg.darkdelay] * 1000) )
    dark = 0;
  if( log_has_event(uigraph.bt_events, time_map[uicfg.darkdelay] * 1000) )
    dark = 0;
  if( log_has_event(uigraph.wifi_events, time_map[uicfg.darkdelay] * 1000) )
    dark = 0;
  if( log_has_event(uigraph.cell_events, time_map[uicfg.darkdelay] * 1000) )
    dark = 0;

  if( dark == 0 && uimon.status == UI_DARK ) {
    if( uicfg.alarmon ) {
      uimon.status = UI_ALRM;
      buzzer(1);
      buzzer(1);
    } else {
      uimon.status = UI_NOTE;
      if( uicfg.notifyon ) {
	buzzer(1); // write twice, is the protocol flakey?
	buzzer(1);
      }
    }
    uimon.state_elapsed_time = 0;
  }
  if( uimon.status == UI_LIVE_DEBOUNCE ) {
    if( dark == 0 )
      uimon.status = UI_LIVE;
    else {
      if( ST2MS(chVTTimeElapsedSinceX( uimon.state_debounce_time )) > time_map[uicfg.darkdelay] * 1000 ) {
	uimon.status = UI_DARK;
	uimon.state_elapsed_time = 0;
      }
    }
  }
  if( dark == 1 && uimon.status == UI_LIVE ) {
    uimon.status = UI_LIVE_DEBOUNCE;
    uimon.state_debounce_time = chVTGetSystemTime();
  }
    
  // now implement UI_ALRM, UI_NOTE transitions, using the elapsed function
  if( uimon.status == UI_ALRM ) {
    buzzer(1);
    if( uimon.state_elapsed_time > time_map[uicfg.alarmtime] ) {
      uimon.status = UI_LIVE;
      uimon.state_elapsed_time = 0;
      buzzer(0);
      buzzer(0);
      buzzer(0);
    }
  }
  if( uimon.status == UI_NOTE ) {
    buzzer(1);
    if( uimon.state_elapsed_time > NOTIFY_TIME ) {
      uimon.status = UI_LIVE;
      uimon.state_elapsed_time = 0;
      buzzer(0);
      buzzer(0);
      buzzer(0);
    }
  }
  
  gdispFlush();
  oledGfxEnd();
  chMtxUnlock(&uigraph.log_mutex);  ////////// unlock uigraph
  
}

static void seconds_cb(void *arg) {
  (void) arg;
  chSysLockFromISR();
  chVTSetI(&seconds_vt, MS2ST(1000), seconds_cb, NULL);
  uimon.state_elapsed_time++;
  chSysUnlockFromISR();
}

static void ui_cb(void *arg) {
  (void) arg;
  chSysLockFromISR();
  chEvtBroadcastI(&ui_timer_event);
  chVTSetI(&ui_vt, MS2ST(60), ui_cb, NULL);
  chSysUnlockFromISR();
}

#define BT_SD    (&SD1)
#define WIFI_SD  (&SD2)

void uiStart(void) {
  int i;
  
  chEvtObjectInit(&ui_timer_event);
  chVTSet(&ui_vt, MS2ST(60), ui_cb, NULL);

  chVTSet(&seconds_vt, MS2ST(1000), seconds_cb, NULL);

  uimon.gps_ok = UI_NOT_OK;
  uimon.cell_ok = UI_NOT_OK;
  uimon.wifi_ok = UI_NOT_OK;
  uimon.bt_ok = UI_NOT_OK;
  uimon.status = UI_LIVE;
  uimon.state_elapsed_time = 0;
  uimon.state_debounce_time = ST2MS(chVTGetSystemTime()) / 1000;
  
  chMtxObjectInit(&uigraph.log_mutex);
  chMtxLock(&uigraph.log_mutex);
  for( i = 0; i < LOGLEN; i++ ) {
    uigraph.gps_events[i] = 0;
    uigraph.bt_events[i] = 0;
    uigraph.wifi_events[i] = 0;
    uigraph.cell_events[i] = 0;
  }
  uigraph.log_index = 0;
  
  uigraph.last_log_time = chVTGetSystemTime(); // time in system time
  chMtxUnlock(&uigraph.log_mutex);

  // set config defaults
  uicfg.simsel = 1;
  uicfg.selftest = 0;
  uicfg.notifyon = 0; // 0 for testing, 1 for production
  uicfg.alarmon = 0; // 0 for testing, 1 for production
  uicfg.alarmtime = 1;
  uicfg.darkdelay = 0;
  uicfg.log_interval = 2000;

}
