#include "Kalman1D.h"

Kalman1D::Kalman1D(float q, float r, float p, float x0)
  : Q(q), R(r), P(p), X(x0) {}

void Kalman1D::setProcessNoise(float q) { Q = q; }
void Kalman1D::setMeasurementNoise(float r) { R = r; }
void Kalman1D::setEstimatedError(float p) { P = p; }

float Kalman1D::updateEstimate(float measurement) {
  // Predict
  float P_prior = P + Q;

  // Kalman gain
  float K = P_prior / (P_prior + R);

  // Update estimate with measurement
  X = X + K * (measurement - X);

  // Update error covariance
  P = (1 - K) * P_prior;

  return X;
}