/* The watch face to tell all available Pebble inforamtion in pure text.
 23.04.2017, version 2.3, Thomas Meier
 accerleration calculation in float, Test result: works correct
 23.04.2017, version 2.2, Thomas Meier
 negative accelerations, valid compass value a little more to right side
 resource optimization, Test result: works fine
 22.04.2017, version 2.1, Thomas Meier
 missing ° added to compass calibration mode,
 trigger compass update if changed by 5 degree
 implementation of square root for acceleration
 Test result: works fine
 22.04.2017, version 2.0, Thomas Meier
 compass is viewed in any case, accelleration max/nim is reset every 15s
 test result: every thing is working like specified
 09.04.2017, version 1.11, Thomas Meier
 compass prepared, Test result: Pebble is endles calibrating
 09.04.2017, version 1.10, Thomas Meier
 bugfix of overflow in final values possible
 28.03.2017, version 1.9, Thomas Meier
 show accerlation data (calculalte magnitude of x,y,z -> min, max) - bug: overflow in final values possible
 26.03.2017, version 1.8, Thomas Meier
 show accerlation data (max and min for selectable x,y,z)
 25.03.2017, version 1.7, Thomas Meier
 improve layout to get easy access to accelleration values, bug in filter fixed
 show accerlation data (unfiltered x,y,z) -> bug in filter
 12.03.2017, version 1.6, Thomas Meier
 show accerlation data (not complete)
 12.03.2017, version 1.5, Thomas Meier
 show the Bluetooth state, layout improvments
 04.01.2017, version 1.4, Thomas Meier
 show the date in format DD:MM NN
 04.01.2017, version 1.3.1, Thomas Meier
 bug fix of time in text format HH:MM:SS (left alligned)
 31.12.2016, version 1.3, Thomas Meier
 time in text format HH:MM:SS
 31.12.2016, version 1.2, Thomas Meier
 battery state in text format %
 11.12.2016, version 1.1, Thomas Meier
 first attempt to add the battery state in text format %
 20.11.2016, version 1.0, Thomas Meier
 find the rigth font - print five text lines with default text
 */

#include <pebble.h>

/* Configuration */
/* Requirement 1: font high shall be > 37 dots */
#define FONT_U40 FONT_KEY_BITHAM_30_BLACK
//#define FONT_U40 FONT_KEY_GOTHIC_18
#define FONT_U40small FONT_KEY_GOTHIC_28_BOLD



/* ************* */

/* Global variables */
static Window *s_pointerToMainWindow;
static TextLayer *s_pointerToTextLayer0;  /* BlueTooth state */
static TextLayer *s_pointerToTextLayer1;  /* Battery state   */
static TextLayer *s_pointerToTextLayer2;  /* Time */
static TextLayer *s_pointerToTextLayer3;  /* Date */
static TextLayer *s_pointerToTextLayer3r; /* Name of day*/
static TextLayer *s_pointerToTextLayer4;  /* Actual Acceleration */
static TextLayer *s_pointerToTextLayer4r; /* Maximum Acceleration */
static TextLayer *s_pointerToTextLayer5;  /* Minium Acceleration */
static TextLayer *s_pointerToTextLayer5r; /* Compas */
static char s_stringBatteryState[8];
static int s_BatteryLevel; /* physical range 0%...100%, resolution 1% */
static char s_stringDayMonth[6], s_stringDayName[4], s_stringShortDayName[3];
static char s_buffer[10];
static char s_stringAccelerationX[7];
static char s_stringAccelerationY[7];
static char s_stringAccelerationZ[7];
static int8_t s_acceleration_8bit, s_acceleration_min8bit, s_acceleration_max8bit;
static int16_t s_acceleration_max16bit;
static int16_t s_acceleration_min16bit;
static float s_acceleration_1g_float;
static char s_stringDegree[7];
static char s_15SecondCounter;

/* ************* */
static float MathFunction_SquareRoot_float(float parameterValue){
    /* Heron Algorithm, Wikipedia: X0 = 0.5(1 + a); Xn+1 = 0.5(Xn + a/Xn) */
    /* with Xn->infiity = square rute of valie a                          */
    
    float x, x_old; /* root value        */
    char i;         /* iteration counter */
    
    if (parameterValue > 0.0){
        x_old = 0; /* initialization value must not be equal to x */
        x = (parameterValue + 1.0)/2.0; /* start value */
        i = 0;
        while( (i <= 40) && ((x_old - x) < 0.008) ){
            i++;
            x_old = x;
            x = (x_old + (parameterValue/x_old))/2.0; /* root iteration value */
        }
    }else{
        /* negative value or zero, root of negative values is not a part of natural numbers */
        x = 0; /* return a zero as root of zero or fault value*/
    }
    return(x);
}
static void Procedure_UpdateDate() {
    /* Requirement 5: show the date in format DD:MM NN */
    
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    strftime(s_stringDayName, sizeof(s_stringDayName), "%a", t);
    strftime(s_stringDayMonth, sizeof(s_stringDayMonth), "%d.%m", t);
    snprintf(s_stringShortDayName, sizeof(s_stringShortDayName), "%s",s_stringDayName);
    text_layer_set_text(s_pointerToTextLayer3, s_stringDayMonth);
    text_layer_set_text(s_pointerToTextLayer3r, s_stringShortDayName);
}
static void Procedure_DateHandler(struct tm *tick_time, TimeUnits units_changed) {
    /* Call procedure which draws the date */
    Procedure_UpdateDate();
}

static void Procedure_UpdateTime() {
    /* Requirement 2: show the time in format HH:MM:SS */
    
    /* Get a tm structure */
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);
    
    /* Write the current hours and minutes into a buffer */
    strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ?
             "%H:%M:%S" : "%I:%M:%S", tick_time);
    
    /* Display this time on the TextLayer */
    text_layer_set_text(s_pointerToTextLayer2, s_buffer);
    
    /* Run second counter */
    if (s_15SecondCounter <=15){
        s_15SecondCounter++;
    }else{
        s_15SecondCounter=0; /* start new 15s cycle */
        /* reset maximum and minimum of acceleration */
        s_acceleration_max8bit = -128;
        s_acceleration_min8bit = 127;
    }
    
}
static void Procedure_updateCompassValue(CompassHeadingData heading_data){
    /*  Requirement 9: show the compas value */
    if(heading_data.compass_status == CompassStatusCalibrated){
        snprintf(s_stringDegree, sizeof(s_stringDegree), " %ld°",TRIGANGLE_TO_DEG(TRIG_MAX_ANGLE - heading_data.magnetic_heading));
        text_layer_set_text(s_pointerToTextLayer5r, s_stringDegree);
        
    }else if(heading_data.compass_status == CompassStatusCalibrating){
        //text_layer_set_text(s_pointerToTextLayer5r, "C");
        snprintf(s_stringDegree, sizeof(s_stringDegree), "c%ld°",TRIGANGLE_TO_DEG(TRIG_MAX_ANGLE - heading_data.magnetic_heading));
        text_layer_set_text(s_pointerToTextLayer5r, s_stringDegree);
        
    }else if(heading_data.compass_status == CompassStatusDataInvalid ){
        snprintf(s_stringDegree, sizeof(s_stringDegree), "?%ld°",TRIGANGLE_TO_DEG(TRIG_MAX_ANGLE - heading_data.magnetic_heading));
        text_layer_set_text(s_pointerToTextLayer5r, s_stringDegree);
        
    }else if(heading_data.compass_status == CompassStatusUnavailable ){
        snprintf(s_stringDegree, sizeof(s_stringDegree), "-%ld°",TRIGANGLE_TO_DEG(TRIG_MAX_ANGLE - heading_data.magnetic_heading));
        text_layer_set_text(s_pointerToTextLayer5r, s_stringDegree);
        
    }else{
        text_layer_set_text(s_pointerToTextLayer5r, "F");
    }
}

static void Procedure_UpdateAcceleration(){
    /*  Requirement 6: show the actual acceleration */
    
    /* local data definitions */
    AccelData l_AccelData_acceleration = (AccelData) { .x = 0, .y = 0, .z = 0 };
    float l_float_accelerationX, l_float_accelerationY, l_float_accelerationZ;
    float l_float_t_magnitude;
    //int32_t l_int32_t_magnitude;
    //int32_t l_int32_test;
    //int8_t l_int8_test;
    int16_t l_int16_t_magnitude;
    
    /* Get acceleration data. Decimal range -32768 ... 32767, resolution 1g/1000, physical range -32.768g ... 32.767g */
    /* x axis: meassured acceleration static 1012 / -1052 (g) */
    /* x axis: meassured acceleration dynamic 2254 / -2650 (shaking, fall down 15cm) */
    /* y axis: meassured acceleration static 1082 / -1006 (g) */
    /* y axis: meassured acceleration dynamic 1202 / -1808 (shaking, fall down 15cm) */
    /* z axis: meassured acceleration static 1070 / -1044 (g) */
    /* z axis: meassured acceleration dynamic 2722 / -1502 (shaking, fall down 15cm) */
    
    /* Read the acceleration values */
    accel_service_peek(&l_AccelData_acceleration);
    //l_AccelData_acceleration.x = 1044;
    //l_AccelData_acceleration.y = 1044;
    //l_AccelData_acceleration.z = 1044;
    
    
    /* ### Calculation part ### */
    if (l_AccelData_acceleration.did_vibrate){
        /* vibration alarm was active, bad data, do nothing */
    }else{
        /* good data */
        /* Limit to minimum or maximum */
        if(l_AccelData_acceleration.x < -4095){l_AccelData_acceleration.x = -4095;}
        if(l_AccelData_acceleration.x > 4095){l_AccelData_acceleration.x = 4095;}
        if(l_AccelData_acceleration.y < -4095){l_AccelData_acceleration.y = -4095;}
        if(l_AccelData_acceleration.y > 4095){l_AccelData_acceleration.y = 4095;}
        if(l_AccelData_acceleration.z < -4095){l_AccelData_acceleration.z= -4095;}
        if(l_AccelData_acceleration.z > 4095){l_AccelData_acceleration.z = 4095;}
        
        /* calculate accelertion with floating point, resolution 1g, g is 9.81m/s^2 */
        l_float_accelerationX = (float)l_AccelData_acceleration.x / s_acceleration_1g_float;
        l_float_accelerationY = (float)l_AccelData_acceleration.y / s_acceleration_1g_float;
        l_float_accelerationZ = (float)l_AccelData_acceleration.z / s_acceleration_1g_float;
        
        /* scale to display value for fixed point calculation */
        //l_AccelData_acceleration.x = l_AccelData_acceleration.x/16;
        //l_AccelData_acceleration.y = l_AccelData_acceleration.y/16;
        //l_AccelData_acceleration.z = l_AccelData_acceleration.z/16;
        
        /* Calculate magnitude of (x,y,z) */
        /* magnitude = sqrt(x^2+y^2+z^2) - earth acceleration */
        /* fixed point */
        //l_int32_t_magnitude = l_AccelData_acceleration.x*l_AccelData_acceleration.x;
        //l_int32_t_magnitude += l_AccelData_acceleration.y*l_AccelData_acceleration.y;
        //l_int32_t_magnitude += l_AccelData_acceleration.z*l_AccelData_acceleration.z;
        //l_int32_t_magnitude = 30000;
        //l_int32_test = l_int32_t_magnitude;
        //l_int32_t_magnitude = MathFunction_SquareRoot(l_int32_t_magnitude);
        //l_int16_t_magnitude = l_int32_t_magnitude/*/32*/; /* scale to display value */
        //l_int16_t_magnitude -= 1044/16; /* remove earth acceleration */
        //if(l_int16_t_magnitude<0){l_int16_t_magnitude=0;} /* no negative values */
        /* foating point */
        l_float_t_magnitude = l_float_accelerationX*l_float_accelerationX;
        l_float_t_magnitude += l_float_accelerationY*l_float_accelerationY;
        l_float_t_magnitude += l_float_accelerationZ*l_float_accelerationZ;
        l_float_t_magnitude = MathFunction_SquareRoot_float(l_float_t_magnitude);
        l_float_t_magnitude -= 1.0; /* remove earth acceleration (1g) */
        l_float_t_magnitude *= 65.25; /* scale to display value (1g=65.25) */
        if     (l_float_t_magnitude>(float)INT8_MAX){l_float_t_magnitude = (float)INT8_MAX;}
        else if(l_float_t_magnitude<(float)INT8_MIN){l_float_t_magnitude = (float)INT8_MIN;}
        s_acceleration_8bit = (int8_t)(l_float_t_magnitude);
        
        /* Hold maximum */
        //if (l_int16_t_magnitude > s_acceleration_max16bit){
        //  s_acceleration_max16bit = l_int16_t_magnitude;
        //}
        if (s_acceleration_8bit > s_acceleration_max8bit){
            s_acceleration_max8bit = s_acceleration_8bit;
        }
        /* Hold minimum */
        //if (l_int16_t_magnitude < s_acceleration_min16bit){
        //  s_acceleration_min16bit = l_int16_t_magnitude;
        //}
        if (s_acceleration_8bit < s_acceleration_min8bit){
            s_acceleration_min8bit = s_acceleration_8bit;
        }
        
        /* Scale to 0...99 */
        //if     (l_int16_t_magnitude>INT8_MAX){l_int16_t_magnitude = INT8_MAX;}
        //else if(l_int16_t_magnitude<INT8_MIN){l_int16_t_magnitude = INT8_MIN;}
        //s_acceleration_8bit = (int8_t)(l_int16_t_magnitude/*/256*/); /* limitation 0 ... 99 */
        //if     (s_acceleration_min16bit>INT8_MAX){s_acceleration_min16bit = INT8_MAX;}
        //else if(s_acceleration_min16bit<INT8_MIN){s_acceleration_min16bit = INT8_MIN;}
        //s_acceleration_min8bit = (int8_t)(s_acceleration_min16bit/*/256*/); /* 25 = 1g */
        //if     (s_acceleration_max16bit>INT8_MAX){s_acceleration_max16bit = INT8_MAX;}
        //else if(s_acceleration_max16bit<INT8_MIN){s_acceleration_max16bit = INT8_MIN;}
        //s_acceleration_max8bit = (int8_t)(s_acceleration_max16bit/*/256*/); /* 25 = 1g */
        
        /* print to sreen */
        snprintf(s_stringAccelerationX, sizeof(s_stringAccelerationX), "%ig",s_acceleration_8bit);
        //snprintf(s_stringAccelerationX, sizeof(s_stringAccelerationX), "%ld",l_int32_test);
        text_layer_set_text(s_pointerToTextLayer4, s_stringAccelerationX);
        snprintf(s_stringAccelerationY, sizeof(s_stringAccelerationY), "%ig",s_acceleration_max8bit);
        //snprintf(s_stringAccelerationY, sizeof(s_stringAccelerationY), "%ig",l_int8_test);
        text_layer_set_text(s_pointerToTextLayer4r, s_stringAccelerationY);
        snprintf(s_stringAccelerationZ, sizeof(s_stringAccelerationZ), "%ig",s_acceleration_min8bit);
        text_layer_set_text(s_pointerToTextLayer5, s_stringAccelerationZ);
    }
}
static void Procedure_TickHandler(struct tm *tick_time, TimeUnits units_changed) {
    /* Call procedure which draws the time */
    Procedure_UpdateTime();
    /* Call procecdure which draws the acceleration values */
    Procedure_UpdateAcceleration();
}

static void Procedure_DrawBatteryState(void) {
    /* Requirement 2: show the battery state in % at right top corner */
    if (s_BatteryLevel>=100){
        snprintf(s_stringBatteryState, sizeof(s_stringBatteryState), "99%%");
    }else{
        snprintf(s_stringBatteryState, sizeof(s_stringBatteryState), "%2i%%", s_BatteryLevel);
    }
    text_layer_set_text(s_pointerToTextLayer1, s_stringBatteryState);
}

static void Callback_SetBatteryState(BatteryChargeState state) {
    /* Write the new battery level to the global variable */
    s_BatteryLevel = state.charge_percent;
    /* Trigger the update of the watchface screen */
    Procedure_DrawBatteryState();
}

static void Procedure_updateBluetoothState(bool connected) {
    /* Requirement 3: show the bluetooth connection (ON/OFF) */
    if(!connected) { /* no connection */
        text_layer_set_text(s_pointerToTextLayer0, "OFF"); /* view OFF */
        vibes_double_pulse(); /* vibration alert */
    }else{ /* connection */
        text_layer_set_text(s_pointerToTextLayer0, "ON"); /* view ON */
        vibes_double_pulse(); /* vibration alert */
    }
}

static void MainWindow_load(Window* parameterPointerToWindow){
    /* get information about Pebble window size */
    Layer *pointerToRootWindowLayer = window_get_root_layer(parameterPointerToWindow);
    GRect boundsOfRootWindow = layer_get_bounds(pointerToRootWindowLayer);
    /* Create text layer */
    s_pointerToTextLayer0 = text_layer_create(GRect(0,   0, boundsOfRootWindow.size.w/2, 34));/* BT state */
    s_pointerToTextLayer1 = text_layer_create(GRect(boundsOfRootWindow.size.w/2+1,   0, boundsOfRootWindow.size.w, 34));/* battery state */
    s_pointerToTextLayer2 = text_layer_create(GRect(0,  34, boundsOfRootWindow.size.w, 34));
    s_pointerToTextLayer3 = text_layer_create(GRect(0,  68, boundsOfRootWindow.size.w-45, 34));
    s_pointerToTextLayer3r = text_layer_create(GRect(boundsOfRootWindow.size.w-44,  68, boundsOfRootWindow.size.w, 34));
    s_pointerToTextLayer4 = text_layer_create(GRect(0, 100, boundsOfRootWindow.size.w/2, 36));
    s_pointerToTextLayer4r = text_layer_create(GRect(boundsOfRootWindow.size.w/2+1, 100, boundsOfRootWindow.size.w/2, 36));
    s_pointerToTextLayer5 = text_layer_create(GRect(0, 132, boundsOfRootWindow.size.w/2, 36));
    //s_pointerToTextLayer5 = text_layer_create(GRect(0, 132, boundsOfRootWindow.size.w, 36));
    s_pointerToTextLayer5r = text_layer_create(GRect(boundsOfRootWindow.size.w/2-20, 132, boundsOfRootWindow.size.w/2+20, 36));
    
    /* Improve the layout to be more like a watchface */
    text_layer_set_background_color(s_pointerToTextLayer0, GColorClear);
    text_layer_set_text_color(s_pointerToTextLayer0, GColorBlack);
    text_layer_set_background_color(s_pointerToTextLayer1, GColorClear);
    text_layer_set_text_color(s_pointerToTextLayer1, GColorBlack);
    text_layer_set_background_color(s_pointerToTextLayer2, GColorClear);
    text_layer_set_text_color(s_pointerToTextLayer2, GColorBlack);
    text_layer_set_background_color(s_pointerToTextLayer3, GColorClear);
    text_layer_set_text_color(s_pointerToTextLayer3, GColorBlack);
    text_layer_set_background_color(s_pointerToTextLayer3r, GColorClear);
    text_layer_set_text_color(s_pointerToTextLayer3r, GColorBlack);
    text_layer_set_background_color(s_pointerToTextLayer4, GColorClear);
    text_layer_set_text_color(s_pointerToTextLayer4, GColorBlack);
    text_layer_set_background_color(s_pointerToTextLayer4r, GColorClear);
    text_layer_set_text_color(s_pointerToTextLayer4r, GColorBlack);
    text_layer_set_background_color(s_pointerToTextLayer5, GColorClear);
    text_layer_set_text_color(s_pointerToTextLayer5, GColorBlack);
    text_layer_set_background_color(s_pointerToTextLayer5r, GColorClear);
    text_layer_set_text_color(s_pointerToTextLayer5r, GColorBlack);
    
    /* Set the default text */
    Procedure_updateBluetoothState(connection_service_peek_pebble_app_connection()); /* right BT state */
    text_layer_set_font(s_pointerToTextLayer0, fonts_get_system_font(FONT_U40));
    text_layer_set_text_alignment(s_pointerToTextLayer0, GTextAlignmentLeft);
    text_layer_set_text(s_pointerToTextLayer1, "99%");
    text_layer_set_font(s_pointerToTextLayer1, fonts_get_system_font(FONT_U40));
    text_layer_set_text_alignment(s_pointerToTextLayer1, GTextAlignmentLeft);
    text_layer_set_text(s_pointerToTextLayer2, "23:22:59");
    text_layer_set_font(s_pointerToTextLayer2, fonts_get_system_font(FONT_U40));
    text_layer_set_text_alignment(s_pointerToTextLayer2, GTextAlignmentLeft);
    text_layer_set_text(s_pointerToTextLayer3, "28.12");
    text_layer_set_font(s_pointerToTextLayer3, fonts_get_system_font(FONT_U40));
    text_layer_set_text_alignment(s_pointerToTextLayer3, GTextAlignmentLeft);
    text_layer_set_text(s_pointerToTextLayer3r, "So");
    text_layer_set_font(s_pointerToTextLayer3r, fonts_get_system_font(FONT_U40));
    text_layer_set_text_alignment(s_pointerToTextLayer3r, GTextAlignmentLeft);
    text_layer_set_text(s_pointerToTextLayer4, "88g");
    text_layer_set_font(s_pointerToTextLayer4, fonts_get_system_font(FONT_U40));
    text_layer_set_text_alignment(s_pointerToTextLayer4, GTextAlignmentLeft);
    text_layer_set_text(s_pointerToTextLayer4r, "88g");
    text_layer_set_font(s_pointerToTextLayer4r, fonts_get_system_font(FONT_U40));
    text_layer_set_text_alignment(s_pointerToTextLayer4r, GTextAlignmentLeft);
    text_layer_set_text(s_pointerToTextLayer5, "88g");
    text_layer_set_font(s_pointerToTextLayer5, fonts_get_system_font(FONT_U40));
    text_layer_set_text_alignment(s_pointerToTextLayer5, GTextAlignmentLeft);
    //text_layer_set_text(s_pointerToTextLayer5r, "?360°");
    text_layer_set_font(s_pointerToTextLayer5r, fonts_get_system_font(FONT_U40));
    text_layer_set_text_alignment(s_pointerToTextLayer5r, GTextAlignmentLeft);
#if defined (PBL_COMPASS)
    text_layer_set_text(s_pointerToTextLayer5r, "?360°");
#else
    text_layer_set_text(s_pointerToTextLayer5r, "NO");
#endif
    
    /* Add it as a child layer to the window's root layer */
    layer_add_child(pointerToRootWindowLayer, text_layer_get_layer(s_pointerToTextLayer0));
    layer_add_child(pointerToRootWindowLayer, text_layer_get_layer(s_pointerToTextLayer1));
    layer_add_child(pointerToRootWindowLayer, text_layer_get_layer(s_pointerToTextLayer2));
    layer_add_child(pointerToRootWindowLayer, text_layer_get_layer(s_pointerToTextLayer3));
    layer_add_child(pointerToRootWindowLayer, text_layer_get_layer(s_pointerToTextLayer3r));
    layer_add_child(pointerToRootWindowLayer, text_layer_get_layer(s_pointerToTextLayer4));
    layer_add_child(pointerToRootWindowLayer, text_layer_get_layer(s_pointerToTextLayer4r));
    layer_add_child(pointerToRootWindowLayer, text_layer_get_layer(s_pointerToTextLayer5));
    layer_add_child(pointerToRootWindowLayer, text_layer_get_layer(s_pointerToTextLayer5r));
}

static void MainWindow_unload(Window* prameterPointerToWindow){
    /* Destroy text layer */
    text_layer_destroy(s_pointerToTextLayer0);
    text_layer_destroy(s_pointerToTextLayer1);
    text_layer_destroy(s_pointerToTextLayer2);
    text_layer_destroy(s_pointerToTextLayer3);
    text_layer_destroy(s_pointerToTextLayer3r);
    text_layer_destroy(s_pointerToTextLayer4);
    text_layer_destroy(s_pointerToTextLayer4r);
    text_layer_destroy(s_pointerToTextLayer5);
    text_layer_destroy(s_pointerToTextLayer5r);
}

static void init(){
    /* initialize static data */
    //s_acceleration_max16bit = -32768;
    //s_acceleration_min16bit = 32767;
    s_acceleration_max8bit = -128;
    s_acceleration_min8bit = 127;
    s_acceleration_1g_float = 1044;
    
    /* Register date update */
    tick_timer_service_subscribe(SECOND_UNIT, Procedure_DateHandler);
    
    /* Register time update */
    tick_timer_service_subscribe(SECOND_UNIT, Procedure_TickHandler);
    
    /* Register callback function to read the battery level updates if state changes */
    battery_state_service_subscribe(Callback_SetBatteryState);
    
    /* create main window */
    s_pointerToMainWindow = window_create();
    
    /* set handler to manage the elements inside the window */
    window_set_window_handlers(s_pointerToMainWindow, (WindowHandlers){
        .load = MainWindow_load,
        .unload = MainWindow_unload
    });
    
    /* show window with animated on */
    window_stack_push(s_pointerToMainWindow, true);
    
    /* Trigger the update of the battery state in the wathchface screen */
    Callback_SetBatteryState(battery_state_service_peek());
    
    /* Call procedure which draws the time */
    Procedure_UpdateTime();
    
    /* Call procedure which draws the date */
    Procedure_UpdateDate();
    
    /* Register for Bluetooth connection updates */
    connection_service_subscribe((ConnectionHandlers) {
        .pebble_app_connection_handler = Procedure_updateBluetoothState});
    
    /* Enable the function accel_service_peek() */
    accel_data_service_subscribe(0, NULL);
    /* Enable the compas */
    compass_service_subscribe(&Procedure_updateCompassValue);
    compass_service_set_heading_filter(DEG_TO_TRIGANGLE(5)); /* trigger update if changed by 5 degree */
}
static void deinit(){
    /* destroy window and all services */
    compass_service_unsubscribe();
    accel_data_service_unsubscribe();
    battery_state_service_unsubscribe();
    connection_service_unsubscribe();
    tick_timer_service_unsubscribe();
    window_destroy(s_pointerToMainWindow);
}
int main(){
    init();
    app_event_loop();
    deinit();
}

