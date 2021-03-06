#include "Bot.h"

Bot::Bot(uint8_t lp1, uint8_t lp2, uint8_t lp3, uint8_t lp4, uint8_t rp1, uint8_t rp2, uint8_t rp3, uint8_t rp4) :
	_diffDrive(DifferentialStepper::HALF4WIRE, lp1, lp3, lp2, lp4, rp1, rp3, rp2, rp4)
{
	_buzzEnd = 0;
	_pauseEnd = 0;
	resetPosition();
}

void Bot::begin()
{
	begin(false);
}

void Bot::begin(bool reverse)
{
	_diffDrive.setMaxStepRate(1000);
	_diffDrive.setAcceleration(2000);
	_diffDrive.setBacklash(STEPS_OF_BACKLASH);
	if (reverse) {
		_diffDrive.setInvertDirectionFor(1, true);
	} else {
		_diffDrive.setInvertDirectionFor(0, true);
	}
}

void Bot::initBuzzer(uint8_t pin)
{
	_pinBuzzer = pin;
	pinMode(_pinBuzzer, OUTPUT);
}

void Bot::initBumpers(uint8_t pinFL, uint8_t pinFR, uint8_t pinBL, uint8_t pinBR, void (*pFunc)(byte collisionData))
{
	_pinSwitchFL = pinFL;
	_pinSwitchFR = pinFR;
	_pinSwitchBL = pinBL;
	_pinSwitchBR = pinBR;

	pinMode(_pinSwitchFL, INPUT_PULLUP);
	pinMode(_pinSwitchFR, INPUT_PULLUP);
	pinMode(_pinSwitchBL, INPUT_PULLUP);
	pinMode(_pinSwitchBR, INPUT_PULLUP);

	bumperCallback = *pFunc;
}

void Bot::initPenLift(uint8_t pin)
{
	_penliftServo.attach(pin);
	penUp();
}

bool Bot::isBusy()
{
	return (!_diffDrive.isQEmpty()) || (millis() < _pauseEnd);
}

bool Bot::isQFull() {
	// make sure we always have two blocks free, to allow space for driveTo commands that need two blocks!
	return _diffDrive.getQueueCapacity() < 3;
}

bool Bot::isQEmpty() {
	return _diffDrive.isQEmpty();
}

void Bot::playStartupJingle()
{
	for (uint8_t i = 0; i < 3; i++) {
		digitalWrite(_pinBuzzer, HIGH);
		delay(100);
		digitalWrite(_pinBuzzer, LOW);
		delay(25);
	}
}

void Bot::run()
{
	// do pause
	if (millis() < _pauseEnd) {
		// do nothing for a while
	} else {
		// Run steppers
		_diffDrive.run();
	}

	// Do buzzer
	if (millis() < _buzzEnd)
		digitalWrite(_pinBuzzer, HIGH);
	else
		digitalWrite(_pinBuzzer, LOW);

	// Collisions
	byte nowColliding = 0;
	if (!digitalRead(_pinSwitchFL)) nowColliding = 1;
	if (!digitalRead(_pinSwitchFR)) nowColliding += 2;
	if (!digitalRead(_pinSwitchBL)) nowColliding += 4;
	if (!digitalRead(_pinSwitchBR)) nowColliding += 8;

	if (nowColliding != state.colliding) {
		// collision state has changed
		if (bumperCallback)
			bumperCallback(nowColliding);

		state.colliding = nowColliding;
	}

	// Disable motors when stopped to save power
	if (!isBusy())
		_diffDrive.disableOutputs();
}

void Bot::penUp()
{
	_penliftServo.write(10);
	pause(100);
}

void Bot::penDown()
{
	_penliftServo.write(90);
	pause(200);
}

void Bot::pause(int len)
{
	// TODO: account for timer overflow
	_pauseEnd = millis() + len;
}

void Bot::buzz(int len)
{
	// TODO: account for timer overflow
	_buzzEnd = millis() + len;
}

void Bot::stop()
{
	_diffDrive.stop();
}

void Bot::emergencyStop()
{
	_diffDrive.emergencyStop();
}

void Bot::enableLookAhead(boolean v) {
	_diffDrive.enableLookAhead(v);
}

void Bot::drive(float distance)
{
	// update state
	state.x += distance * cos(state.ang * DEGTORAD);
	state.y += distance * sin(state.ang * DEGTORAD);

	// prime the move
	long steps = distance * STEPS_PER_MM;
	_diffDrive.queueMove(steps,steps);
}

void Bot::turn(float ang)
{
	// update state
	state.ang += ang;
	correctAngleWrap();

	// prime the move
	long steps = ang * STEPS_PER_DEG;
	_diffDrive.queueMove(-steps,steps);
}

void Bot::drive(float leftDist, float rightDist) {
	// TODO: update state, using reverse of arcTo calculations

	_diffDrive.queueMove(leftDist * STEPS_PER_MM, rightDist * STEPS_PER_MM);
}


void Bot::driveTo(float x, float y) {
  // calc angle
  double ang = atan2(y-state.y, x-state.x) * RADTODEG;
  // now angle delta
  ang = ang - state.ang;
  if (ang > 180)
    ang = -(360 - ang);
  if (ang < -180)
    ang = 360 + ang;

  // turn to face correct direction
  turn(ang);

  // move required distance
  float dist = sqrt((y-state.y)*(y-state.y) + (x-state.x)*(x-state.x));
  drive(dist);
}

void Bot::arcTo (float x, float y)
{
    float cx1 = x - state.x;
    float cy1 = y - state.y;

    //v.rotate(degToRad(-this.state.angle));
    float ang = -state.ang * DEGTORAD + PI / 2;
    float ca = cos(ang);
    float sa = sin(ang);
    float cx = cx1 * ca - cy1 * sa;
    float cy = cx1 * sa + cy1 * ca;

	if (cy == 0) return;

    float m = -cx / cy;

    // calc centre of arc
    // from equation
    // y - y1 = m (x - x1)
    // rearranged to find y axis intersection
    // x = (-y1)/m + x1
    float x1 = -(cy/2.0) / m + (cx/2.0);

    float dl = 0, dr = 0;
    float targetAng;
    float cl, cr;

    if (x1 < 0) {
        targetAng = atan2(cy, -x1 + cx) * RADTODEG;

        cl = 2.0 * PI * (-WHEELSPACING/2.0 - x1);
        dl = cl * targetAng/360.0;

        cr = 2.0 * PI * (WHEELSPACING/2.0 - x1);
        dr = cr * targetAng/360.0;

    } else {
        targetAng = atan2(cy, x1 - cx) * RADTODEG;

        cl = 2.0 * PI * (x1 + WHEELSPACING/2.0 );
        dl = cl * targetAng/360.0;

        cr = 2.0 * PI * (x1 - WHEELSPACING/2.0);
        dr = cr * targetAng/360.0;
    }

	state.x = x;
	state.y = y;
	if (dl < dr)
		state.ang += targetAng;
	else
		state.ang -= targetAng;
	correctAngleWrap();

	_diffDrive.queueMove(dl * STEPS_PER_MM, dr * STEPS_PER_MM);
}

// Draw a circle size dia, direction = -1 clockwise, 1 anticlockwise
void Bot::circle(float dia, float direction)
{
	// Calc circle diameters wheels will have to follow
	float diaR = dia + WHEELSPACING;
	float diaL = dia - WHEELSPACING;

	// Calc distances round circles
	float dr = PI * diaR;
	float dl = PI * diaL;

	// No state change :-)
	if (direction > 0)
		_diffDrive.queueMove(dl * STEPS_PER_MM, dr * STEPS_PER_MM);
	else
		_diffDrive.queueMove(dr * STEPS_PER_MM, dl * STEPS_PER_MM);
}

// position calcs
void Bot::resetPosition() {
	state.x = 0;
	state.y = 0;
	state.ang = 0;
}

// correct wrap around
void Bot::correctAngleWrap()
{
	if (state.ang > 180) state.ang -= 360;
	if (state.ang < -180) state.ang += 360;
}
