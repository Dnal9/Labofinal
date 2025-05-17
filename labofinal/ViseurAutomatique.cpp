#include "ViseurAutomatique.h"
#include <Arduino.h>

ViseurAutomatique::ViseurAutomatique(int p1, int p2, int p3, int p4, float& distanceRef)
  : _stepper(AccelStepper::FULL4WIRE, p1, p3, p2, p4), _distance(distanceRef) {
  _stepper.setMaxSpeed(500);
  _stepper.setAcceleration(100);
}

void ViseurAutomatique::update() {
  _currentTime = millis();

  if (_etat == INACTIF) return;

  if (_distance >= _distanceMinSuivi && _distance <= _distanceMaxSuivi) {
    _etat = SUIVI;
  } else {
    _etat = REPOS;
  }

  if (_etat == SUIVI) {
    float mapped = (_distance - _distanceMinSuivi) / (_distanceMaxSuivi - _distanceMinSuivi);
    float angle = _angleMin + mapped * (_angleMax - _angleMin);
    angle = constrain(angle, _angleMin, _angleMax);
    _stepper.moveTo(_angleEnSteps(angle));
  }

  if (_stepper.distanceToGo() != 0) {
    _stepper.run();
  }
}

void ViseurAutomatique::setAngleMin(float angle) {
  _angleMin = angle;
}

void ViseurAutomatique::setAngleMax(float angle) {
  _angleMax = angle;
}

void ViseurAutomatique::setPasParTour(int steps) {
  _stepsPerRev = steps;
}

void ViseurAutomatique::setDistanceMinSuivi(float distanceMin) {
  _distanceMinSuivi = distanceMin;
}

void ViseurAutomatique::setDistanceMaxSuivi(float distanceMax) {
  _distanceMaxSuivi = distanceMax;
}

float ViseurAutomatique::getAngle() const {
  long pos = _stepper.currentPosition();
  return map(pos, _angleEnSteps(_angleMin), _angleEnSteps(_angleMax), _angleMin, _angleMax);
}

void ViseurAutomatique::activer() {
  _etat = REPOS;
  _stepper.run();
}

void ViseurAutomatique::desactiver() {
  _etat = INACTIF;
  _stepper.stop();
}

const char* ViseurAutomatique::getEtatTexte() const {
  switch (_etat) {
    case INACTIF: return "INACTIF";
    case SUIVI: return "SUIVI";
    case REPOS: return "REPOS";
  }
  return "";
}

long ViseurAutomatique::_angleEnSteps(float angle) const {
  return map(angle, 0, 180, -_stepsPerRev / 2, _stepsPerRev / 2);
}
