#ifndef KALMAN1D_H
#define KALMAN1D_H

class Kalman1D {
public:
  Kalman1D(float q = 1e-5f, float r = 1e-2f, float p = 1e-3f, float x0 = 0.0f);

  void setProcessNoise(float q);
  void setMeasurementNoise(float r);
  void setEstimatedError(float p);

  float updateEstimate(float measurement);

private:
  float Q; // process noise
  float R; // measurement noise
  float P; // estimation error covariance
  float X; // estimated state
};

#endif // KALMAN1D_H