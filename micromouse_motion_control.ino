
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_VL53L0X.h>


// Global variables
Adafruit_MPU6050 mpu;


// Motor pins
const int M1_direction = 4;
const int M1_PWM       = 5;
const int M2_direction = 7;
const int M2_PWM       = 6;

const int optical_sensor = A0;


//distance sensors
Adafruit_VL53L0X sensor_left;
Adafruit_VL53L0X sensor_front;
Adafruit_VL53L0X sensor_right;

#define XSHUT_LEFT   16
#define XSHUT_FRONT  17
#define XSHUT_RIGHT  18

#define ADDRESS_LEFT   0x30
#define ADDRESS_FRONT  0x31
#define ADDRESS_RIGHT  0x32

// PID constants
#define MAX_CORRECTION 300.0
#define PWM_MAX        1000.0
#define ERROR_BAND     0.5
#define INTEGRAL_LIMIT 10.0

double base_pwm = 500;


// IMU variables
double gyro_bias = 0.0;
double angle = 0.0;

unsigned long previous_time;


// --------------------------------------------------
// SETUP
// --------------------------------------------------

void setup()
{
    Serial.begin(9600);

    // I2C
    Wire.begin();


     // --------------------------------------------------
    // VL53L0X INITIALISATION
    // --------------------------------------------------

    pinMode(XSHUT_LEFT, OUTPUT);
    pinMode(XSHUT_FRONT, OUTPUT);
    pinMode(XSHUT_RIGHT, OUTPUT);

    // Turn all sensors off
    digitalWrite(XSHUT_LEFT, LOW);
    digitalWrite(XSHUT_FRONT, LOW);
    digitalWrite(XSHUT_RIGHT, LOW);

    delay(10);


    // LEFT SENSOR
    digitalWrite(XSHUT_LEFT, HIGH);
    delay(10);

    if (!sensor_left.begin(ADDRESS_LEFT))
    {
        Serial.println("Left VL53L0X not found!");
        while (1);
    }


    // FRONT SENSOR
    digitalWrite(XSHUT_FRONT, HIGH);
    delay(10);

    if (!sensor_front.begin(ADDRESS_FRONT))
    {
        Serial.println("Front VL53L0X not found!");
        while (1);
    }


    // RIGHT SENSOR
    digitalWrite(XSHUT_RIGHT, HIGH);
    delay(10);

    if (!sensor_right.begin(ADDRESS_RIGHT))
    {
        Serial.println("Right VL53L0X not found!");
        while (1);
    }

    Serial.println("All VL53L0X sensors found!");

    // --------------------------------------------------
    // MPU-6050 INITIALISATION
    // --------------------------------------------------

    if (!mpu.begin(0x68))
    {
        Serial.println("MPU6050 not found!");
        while (1);
    }

    Serial.println("MPU6050 found!");

    // Gyroscope range
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);

    // Filter
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);


    // --------------------------------------------------
    // MOTOR INITIALISATION
    // --------------------------------------------------

    pinMode(M1_direction, OUTPUT);
    pinMode(M1_PWM, OUTPUT);

    pinMode(M2_direction, OUTPUT);
    pinMode(M2_PWM, OUTPUT);

    pinMode(2, OUTPUT);


    // --------------------------------------------------
    // GYRO CALIBRATION
    // --------------------------------------------------

    Serial.println("Keep mouse completely still...");
    delay(2000);

    calibrate_gyro();

    Serial.println("Calibration complete.");

    previous_time = micros();
}


// --------------------------------------------------
// MAIN LOOP
// --------------------------------------------------

void loop()
{

    //test motors//
        // Motor speeds calculated by PID
        double left_pwm=500;
        double right_pwm=500;
           // Drive both motors forward
        digitalWrite(M1_direction, HIGH);
        digitalWrite(M2_direction, HIGH);

        // Apply PID-adjusted speeds
        analogWrite(M1_PWM, left_pwm);
        analogWrite(M2_PWM, right_pwm);
    //////////////////////////////////

    //test turning//////////////////////
    turn_right(90);

    delay(1000);

    turn_right(180);


    turn_left(90);

    delay(1000);

    turn_left(180);

/////////////////////////////////////

//////arc turns////////////////////
        arc_turn_right(90);

        delay(1000);

        arc_turn_left(90);

////////////////////////////////////



///////////PID test/////////////////


drive_forward();

///////////////////////////////////
}


// --------------------------------------------------
// GYRO CALIBRATION
// --------------------------------------------------

void calibrate_gyro()
{
    double total = 0.0;

    const int samples = 500;

    for (int i = 0; i < samples; i++)
    {
        sensors_event_t accel;
        sensors_event_t gyro;
        sensors_event_t temp;

        mpu.getEvent(&accel, &gyro, &temp);

        // Gyro Z is in radians/second
        total += gyro.gyro.z;

        delay(5);
    }

    gyro_bias = total / samples;

    Serial.print("Gyro bias: ");
    Serial.println(gyro_bias);
}


// --------------------------------------------------
// UPDATE ORIENTATION
// --------------------------------------------------

void update_orientation()
{
    sensors_event_t accel;
    sensors_event_t gyro;
    sensors_event_t temp;

    mpu.getEvent(&accel, &gyro, &temp);


    // Calculate elapsed time
    unsigned long current_time = micros();

    double dt = (current_time - previous_time) / 1000000.0;

    previous_time = current_time;


    // Remove gyro bias
    double gyro_z = gyro.gyro.z - gyro_bias;


    // Convert rad/s to degrees/s
    double gyro_degrees = gyro_z * 180.0 / PI;


    // Integrate gyro to obtain angle
    angle += gyro_degrees * dt;


    // Keep angle between 0 and 360 degrees
    if (angle >= 360.0)
        angle -= 360.0;

    if (angle < 0.0)
        angle += 360.0;
}


// --------------------------------------------------
// PID CONTROLLER
// --------------------------------------------------

void pid_controller(double target_orientation,
                    double current_orientation,
                    double base_pwm,
                    double *left_pwm,
                    double *right_pwm,
                    double *integral,
                    double *prev_error)
{
    double kp = 10.0;
    double ki = 20.0;
    double kd = 0.05;
    double dt = 0.001;

    double error;
    double derivative;
    double correction;


    *left_pwm = base_pwm;
    *right_pwm = base_pwm;


    error = current_orientation - target_orientation;


    // Inside the +-0.5 degree band
    if (fabs(error) < ERROR_BAND)
    {
        *prev_error = error;

        analogWrite(M1_PWM, *left_pwm);
        analogWrite(M2_PWM, *right_pwm);

        return;
    }


    *integral += error * dt;


    if (*integral > INTEGRAL_LIMIT)
        *integral = INTEGRAL_LIMIT;

    if (*integral < -INTEGRAL_LIMIT)
        *integral = -INTEGRAL_LIMIT;


    derivative = (error - *prev_error) / dt;

    *prev_error = error;


    correction = kp * error
               + ki * (*integral)
               + kd * derivative;


    if (error < 0.0)
        correction = -correction;


    if (correction < 0.0)
        correction = 0.0;

    if (correction > MAX_CORRECTION)
        correction = MAX_CORRECTION;


    if (error > 0.0)
        *right_pwm += correction;
    else
        *left_pwm += correction;


    if (*left_pwm > PWM_MAX)
        *left_pwm = PWM_MAX;

    if (*right_pwm > PWM_MAX)
        *right_pwm = PWM_MAX;


    analogWrite(M1_PWM, *left_pwm);
    analogWrite(M2_PWM, *right_pwm);
}


//uses the PID to mke it drive straight by checking the difference between current gyro reading and desired gyro reading
void drive_forward()
{
    // Set the target orientation to the direction
    // the mouse is facing when it starts
    double target_orientation = orientation();

    // PID variables
    double integral = 0.0;
    double prev_error = 0.0;

    while (true)
    {
        // Update IMU
        update_orientation();

        // Get current orientation
        double current_orientation = orientation();

        // Motor speeds calculated by PID
        double left_pwm;
        double right_pwm;

        // Run PID controller
        pid_controller(
            target_orientation,
            current_orientation,
            base_pwm,
            &left_pwm,
            &right_pwm,
            &integral,
            &prev_error
        );

        // Drive both motors forward
        digitalWrite(M1_direction, HIGH);
        digitalWrite(M2_direction, HIGH);

        // Apply PID-adjusted speeds
        analogWrite(M1_PWM, left_pwm);
        analogWrite(M2_PWM, right_pwm);
    }
}
// --------------------------------------------------
// TURN RIGHT
// --------------------------------------------------

void turn_right(int degrees)
{
    int starting_orientation = orientation();

    int goal_orientation = starting_orientation + degrees;


    if (goal_orientation > 359)
        goal_orientation -= 360;


    if (starting_orientation > goal_orientation)
    {
        while (orientation() > 10)
        {
            update_orientation();

            digitalWrite(M1_direction, HIGH);
            digitalWrite(M2_direction, HIGH);

            analogWrite(M1_PWM, base_pwm);
            analogWrite(M2_PWM, 0);
        }
    }


    while (orientation() < goal_orientation)
    {
        update_orientation();

        digitalWrite(M1_direction, HIGH);
        digitalWrite(M2_direction, HIGH);

        analogWrite(M1_PWM, base_pwm);
        analogWrite(M2_PWM, 0);
    }


    stop_motors();
}


//arc turn right
void arc_turn_right(int degrees)
{
    int starting_orientation = orientation();

    int goal_orientation = starting_orientation + degrees;


    if (goal_orientation > 359)
        goal_orientation -= 360;


    if (starting_orientation > goal_orientation)
    {
        while (orientation() > 10)
        {
            update_orientation();

            digitalWrite(M1_direction, HIGH);
            digitalWrite(M2_direction, HIGH);

            analogWrite(M1_PWM, base_pwm);
            analogWrite(M2_PWM, 100); //value needs testing
        }
    }


    while (orientation() < goal_orientation)
    {
        update_orientation();

        digitalWrite(M1_direction, HIGH);
        digitalWrite(M2_direction, HIGH);

        analogWrite(M1_PWM, base_pwm);
        analogWrite(M2_PWM, 100);  //value needs testing
    }


    stop_motors();
}


// --------------------------------------------------
// TURN LEFT
// --------------------------------------------------

void turn_left(int degrees)
{
    int starting_orientation = orientation();

    int goal_orientation = starting_orientation - degrees;


    if (goal_orientation < 0)
        goal_orientation += 360;


    if (starting_orientation < goal_orientation)
    {
        while (orientation() <= 350)
        {
            update_orientation();

            digitalWrite(M1_direction, HIGH);
            digitalWrite(M2_direction, HIGH);

            analogWrite(M1_PWM, 0);
            analogWrite(M2_PWM, base_pwm);
        }
    }


    while (orientation() > goal_orientation)
    {
        update_orientation();

        digitalWrite(M1_direction, HIGH);
        digitalWrite(M2_direction, HIGH);

        analogWrite(M1_PWM, 0);
        analogWrite(M2_PWM, base_pwm);
    }


    stop_motors();
}


void arc_turn_left(int degrees)
{
    int starting_orientation = orientation();

    int goal_orientation = starting_orientation - degrees;


    if (goal_orientation < 0)
        goal_orientation += 360;


    if (starting_orientation < goal_orientation)
    {
        while (orientation() <= 350)
        {
            update_orientation();

            digitalWrite(M1_direction, HIGH);
            digitalWrite(M2_direction, HIGH);

            analogWrite(M1_PWM, 100);//value needs testing
            analogWrite(M2_PWM, base_pwm);
        }
    }


    while (orientation() > goal_orientation)
    {
        update_orientation();

        digitalWrite(M1_direction, HIGH);
        digitalWrite(M2_direction, HIGH);

        analogWrite(M1_PWM, 100);//value needs testing
        analogWrite(M2_PWM, base_pwm);
    }


    stop_motors();
}

// --------------------------------------------------
// STOP MOTORS
// --------------------------------------------------

void stop_motors()
{
    analogWrite(M1_PWM, 0);
    analogWrite(M2_PWM, 0);
}


// --------------------------------------------------
// ORIENTATION
// --------------------------------------------------

int orientation()
{
    return (int)angle;
}