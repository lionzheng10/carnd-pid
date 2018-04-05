#include <uWS/uWS.h>
#include <iostream>
#include "json.hpp"
#include "PID.h"
#include <math.h>
#include <limits>
#include <chrono>
#include <ctime>

// for convenience
using json = nlohmann::json;

// For converting back and forth between radians and degrees.
constexpr double pi() { return M_PI; }
double deg2rad(double x) { return x * pi() / 180; }
double rad2deg(double x) { return x * 180 / pi(); }

auto start = std::chrono::system_clock::now();

// Checks if the SocketIO event has JSON data.
// If there is data the JSON object in string format will be returned,
// else the empty string "" will be returned.
std::string hasData(std::string s) {
  auto found_null = s.find("null");
  auto b1 = s.find_first_of("[");
  auto b2 = s.find_last_of("]");
  if (found_null != std::string::npos) {
    return "";
  }
  else if (b1 != std::string::npos && b2 != std::string::npos) {
    return s.substr(b1, b2 - b1 + 1);
  }
  return "";
}

int main()
{
  uWS::Hub h;

  // steering and throttle PID controllers
  PID pid_s, pid_t;
  double last_steer = 0;
  // TODO: Initialize the pid variable.
  pid_s.Init(0.03, 0.0015, 1.0);
  pid_t.Init(0.2, 0.0000, 0.0);
  
  start = std::chrono::system_clock::now();

  h.onMessage([&pid_s, &pid_t, &last_steer](uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length, uWS::OpCode opCode) {
    // "42" at the start of the message means there's a websocket message event.
    // The 4 signifies a websocket message
    // The 2 signifies a websocket event
    if (length && length > 2 && data[0] == '4' && data[1] == '2')
    {
      auto s = hasData(std::string(data).substr(0, length));
      if (s != "") {
        auto j = json::parse(s);
        std::string event = j[0].get<std::string>();

        //debug display s value
        //std::cout << j[0] << std::endl;
        
        if (event == "telemetry") {
          // j[1] is the data JSON object
          double cte = std::stod(j[1]["cte"].get<std::string>());
          double speed = std::stod(j[1]["speed"].get<std::string>());
          //double angle = std::stod(j[1]["steering_angle"].get<std::string>());
          double steer_value, throttle_value;
          double P_part, I_part, D_part ,P_add;
          /*
          * TODO: Calcuate steering value here, remember the steering value is
          * [-1, 1].
          * NOTE: Feel free to play around with the throttle and speed. Maybe use
          * another PID controller to control the speed!
          */

          // update error and calculate steer_value at each step
          pid_s.UpdateError(cte);

          if (cte > 2) {
            P_add = -0.1;
          } else if (cte < -2) {
            P_add = 0.1;
          } else{
            P_add = 0;
          }

          steer_value = - pid_s.Kp * pid_s.p_error 
                        - pid_s.Kd * pid_s.d_error 
                        - pid_s.Ki * pid_s.i_error
                        + P_add;

          P_part = - pid_s.Kp * pid_s.p_error;
          I_part = - pid_s.Ki * pid_s.i_error;
          D_part = - pid_s.Kd * pid_s.d_error;

          // output smooth
          double max_change = 0.12;
          if (steer_value < (last_steer - max_change)){
            steer_value = last_steer - max_change;
          }else if(steer_value > (last_steer + max_change)){
            steer_value = last_steer + max_change;
          }

          // output limit
          if (steer_value > 0.5) {
            steer_value = 0.5;
          }
          if (steer_value < -0.5) {
            steer_value = -0.5;
          }
          last_steer= steer_value;

          // update error and calculate throttle_value at each step
          pid_t.UpdateError(fabs(cte));     // |cte|
          throttle_value = 0.75 - pid_t.Kp * pid_t.p_error
                        - pid_t.Kd * pid_t.d_error 
                        - pid_t.Ki * pid_t.i_error;


          if (speed > 65) {
            throttle_value = throttle_value - 0.3;
          }        

          //throttle limit
          if (throttle_value > 0.8){
            throttle_value = 0.8;
          }

          if (throttle_value < -0.2){
            throttle_value = -0.2;
          }


          // DEBUG
          auto end = std::chrono::system_clock::now();
          std::chrono::duration<double> elapsed_seconds = end-start;
          //std::time_t end_time = std::chrono::system_clock::to_time_t(end);

          std::cout << "elapsed time: " << elapsed_seconds.count() << "s\n";//<< "finished computation at " << std::ctime(&end_time)

          // DEBUG
          std::cout << " -------------------------- " << std::endl;
          std::cout << "CTE: " << cte << std::endl;
          std::cout << "pid_s.Kp: " << pid_s.Kp << " pid_s.Ki: " << pid_s.Ki << " pid_s.Kd: " << pid_s.Kd << std::endl; 
          std::cout << "P_part: " << P_part << " I_part:  " << I_part << " D_part:" << D_part << std::endl; 
          std::cout << "steer value: " << steer_value << " throttle_value:  " << throttle_value << std::endl; 

          json msgJson;
          msgJson["steering_angle"] = steer_value;
          msgJson["throttle"] = throttle_value;
          auto msg = "42[\"steer\"," + msgJson.dump() + "]";
          //std::cout << msg << std::endl;
          ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
        }
      } else {
        // Manual driving
        std::string msg = "42[\"manual\",{}]";
        ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
      }
    }
  });

  // We don't need this since we're not using HTTP but if it's removed the program
  // doesn't compile :-(
  h.onHttpRequest([](uWS::HttpResponse *res, uWS::HttpRequest req, char *data, size_t, size_t) {
    const std::string s = "<h1>Hello world!</h1>";
    if (req.getUrl().valueLength == 1)
    {
      res->end(s.data(), s.length());
    }
    else
    {
      // i guess this should be done more gracefully?
      res->end(nullptr, 0);
    }
  });

  h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
    std::cout << "Connected!!!" << std::endl;
  });

  h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code, char *message, size_t length) {
    ws.close();
    std::cout << "Disconnected" << std::endl;
  });

  int port = 4567;
  if (h.listen(port))
  {
    std::cout << "Listening to port " << port << std::endl;
  }
  else
  {
    std::cerr << "Failed to listen to port" << std::endl;
    return -1;
  }
  h.run();
}
