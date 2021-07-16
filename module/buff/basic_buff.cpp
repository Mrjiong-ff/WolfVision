#include "basic_buff.hpp"

namespace basic_buff {

Detector::Detector(const std::string& _buff_config_path) {
  cv::FileStorage buff_config_fs(_buff_config_path, cv::FileStorage::READ);

  readBuffConfig(buff_config_fs);

  buff_config_fs.release();

  target_2d_point_.reserve(4);
  final_target_z_ = 0.f;
  target_rect_    = cv::RotatedRect();
  my_color_       = 0;

  split_img_.reserve(3);
  average_th_ = 0;

  action_cnt_        = 0;
  small_rect_area_   = 0.f;
  small_rect_length_ = 0.f;
  big_rect_area_     = 0.f;
  big_rect_length_   = 0.f;

  small_target_aspect_ratio_max_int_ = buff_config_.param.SMALL_TARGET_ASPECT_RATIO_MAX * 10;
  small_target_aspect_ratio_min_int_ = buff_config_.param.SMALL_TARGET_ASPECT_RATIO_MIN * 10;
  area_ratio_max_int_                = buff_config_.param.AREA_RATIO_MAX * 100;
  area_ratio_min_int_                = buff_config_.param.AREA_RATIO_MIN * 100;

  is_find_target_  = false;

  current_angle_   = 0.f;
  last_angle_      = 0.f;
  diff_angle_      = 0.f;
  is_change_blade_ = false;

  current_direction_ = 0.f;
  last_direction_    = 0.f;
  find_cnt_          = 0;
  d_angle_           = 1.f;
  confirm_cnt_       = 0;
  is_confirm_        = false;

  current_speed_   = 0.f;
  last_time_       = 0.0;
  last_diff_angle_ = 0.f;


  barrel_buff_botton_h_    = (buff_config_.param.BUFF_H - buff_config_.param.BUFF_RADIUS) -
                             (buff_config_.param.PLATFORM_H + buff_config_.param.BARREL_ROBOT_H);
  current_radian_          = 0.f;
  target_buff_h_           = 0.f;
  target_y_                = 0.f;
  target_x_                = 0.f;
  target_z_                = 0.f;
  offset_angle_int_        = 0;
  offset_angle_float_      = 0.f;
  bullet_tof_              = 0.f;
  fixed_forecast_quantity_ = 0.f;
  final_forecast_quantity_ = 0.f;

  theta_       = 0.0;
  final_angle_ = 0.f;
  sin_calcu_   = 0.f;
  cos_calcu_   = 0.f;
  pre_center_  = cv::Point2f(0.f, 0.f);
  radio_       = 0.f;
}

inline void Detector::getInput(cv::Mat& _input_img, const int& _my_color) {
  src_img_  = _input_img;
  my_color_ = _my_color;
  src_img_.copyTo(dst_img_);
  is_find_target_ = false;
}

inline void Detector::displayDst() { imshow("[basic_buff] displayDst() -> dst_img_", dst_img_); }

void Detector::runTask(cv::Mat&            _input_img,
                       uart::Receive_Data& _receive_info,
                       uart::Write_Data&   _send_info) {
  getInput(_input_img, _receive_info.my_color);
  imageProcessing(src_img_, my_color_, BGR_MODE);

  findTarget(dst_img_, bin_img_, target_box_);
  is_find_target_ = isFindTarget(dst_img_, target_box_);
  final_center_r_ = findCircleR(src_img_, bin_img_, dst_img_, is_find_target_);

  judgeCondition(is_find_target_);

  final_forecast_quantity_ =
      doPredict(static_cast<float>(_receive_info.bullet_velocity), is_find_target_);

  fmt::print("[{}] Info, early degrees: {}\n",
             idntifier_green,
             final_forecast_quantity_ * 180 / CV_PI);


  calculateTargetPointSet(final_forecast_quantity_,
                          final_center_r_,
                          target_2d_point_,
                          dst_img_,
                          is_find_target_);

  if (is_find_target_) {
    buff_pnp_.solvePnP(28,
                       2,
                       target_2d_point_,
                       final_target_z_);

    _send_info.yaw_angle   = buff_pnp_.returnYawAngle();
    _send_info.pitch_angle = buff_pnp_.returnPitchAngle();
    _send_info.depth       = buff_pnp_.returnDepth();
    _send_info.data_type   = is_find_target_;

    fmt::print("[{}] Info, yaw: {}, pitch: {}, depth: {}\n",
               idntifier_green,
               _send_info.yaw_angle,
               _send_info.pitch_angle,
               _send_info.depth);
  } else {
    _send_info = uart::Write_Data();
  }

  displayDst();
  updateLastData(is_find_target_);
}

uart::Write_Data Detector::runTask(cv::Mat&            _input_img,
                                   uart::Receive_Data& _receive_info) {
  uart::Write_Data send_info;

  getInput(_input_img, _receive_info.my_color);
  imageProcessing(src_img_, my_color_, BGR_MODE);

  findTarget(dst_img_, bin_img_, target_box_);
  is_find_target_ = isFindTarget(dst_img_, target_box_);
  final_center_r_ = findCircleR(src_img_, bin_img_, dst_img_, is_find_target_);
  judgeCondition(is_find_target_);

  final_forecast_quantity_ =
      doPredict(static_cast<float>(_receive_info.bullet_velocity), is_find_target_);

  fmt::print("[{}] Info, early degrees: {}\n",
             idntifier_green,
             final_forecast_quantity_ * 180 / CV_PI);

  calculateTargetPointSet(final_forecast_quantity_,
                          final_center_r_,
                          target_2d_point_,
                          dst_img_,
                          is_find_target_);

  if (is_find_target_) {
    buff_pnp_.solvePnP(28,
                       2,
                       target_2d_point_,
                       final_target_z_);

    send_info.yaw_angle   = buff_pnp_.returnYawAngle();
    send_info.pitch_angle = buff_pnp_.returnPitchAngle();
    send_info.depth       = buff_pnp_.returnDepth();
    send_info.data_type   = is_find_target_;

    fmt::print("[{}] Info, yaw: {}, pitch: {}, depth: {}\n",
               idntifier_green,
               send_info.yaw_angle,
               send_info.pitch_angle,
               send_info.depth);
  } else {
    send_info = uart::Write_Data();
  }

  displayDst();
  updateLastData(is_find_target_);

  return send_info;
}

void Detector::readBuffConfig(const cv::FileStorage& _fs) {
  _fs["IS_SHOW_BIN_IMG"]     >> buff_config_.ctrl.IS_SHOW_BIN_IMG;
  _fs["PROCESSING_MODE"]     >> buff_config_.ctrl.PROCESSING_MODE;
  _fs["IS_PARAM_ADJUSTMENT"] >> buff_config_.ctrl.IS_PARAM_ADJUSTMENT;

  _fs["RED_BUFF_GRAY_TH"]   >> buff_config_.param.RED_BUFF_GRAY_TH;
  _fs["RED_BUFF_COLOR_TH"]  >> buff_config_.param.RED_BUFF_COLOR_TH;
  _fs["BLUE_BUFF_GRAY_TH"]  >> buff_config_.param.BLUE_BUFF_GRAY_TH;
  _fs["BLUE_BUFF_COLOR_TH"] >> buff_config_.param.BLUE_BUFF_COLOR_TH;

  _fs["H_RED_MAX"] >> buff_config_.param.H_RED_MAX;
  _fs["H_RED_MIN"] >> buff_config_.param.H_RED_MIN;
  _fs["S_RED_MAX"] >> buff_config_.param.S_RED_MAX;
  _fs["S_RED_MIN"] >> buff_config_.param.S_RED_MIN;
  _fs["V_RED_MAX"] >> buff_config_.param.V_RED_MAX;
  _fs["V_RED_MIN"] >> buff_config_.param.V_RED_MIN;

  _fs["H_BLUE_MAX"] >> buff_config_.param.H_BLUE_MAX;
  _fs["H_BLUE_MIN"] >> buff_config_.param.H_BLUE_MIN;
  _fs["S_BLUE_MAX"] >> buff_config_.param.S_BLUE_MAX;
  _fs["S_BLUE_MIN"] >> buff_config_.param.S_BLUE_MIN;
  _fs["V_BLUE_MAX"] >> buff_config_.param.V_BLUE_MAX;
  _fs["V_BLUE_MIN"] >> buff_config_.param.V_BLUE_MIN;

  _fs["SMALL_TARGET_AREA_MAX"] >> buff_config_.param.SMALL_TARGET_AREA_MAX;
  _fs["SMALL_TARGET_AREA_MIN"] >> buff_config_.param.SMALL_TARGET_AREA_MIN;
  _fs["BIG_TARGET_AREA_MAX"]   >> buff_config_.param.BIG_TARGET_AREA_MAX;
  _fs["BIG_TARGET_AREA_MIN"]   >> buff_config_.param.BIG_TARGET_AREA_MIN;

  _fs["SMALL_TARGET_Length_MAX"] >> buff_config_.param.SMALL_TARGET_Length_MAX;
  _fs["SMALL_TARGET_Length_MIN"] >> buff_config_.param.SMALL_TARGET_Length_MIN;
  _fs["BIG_TARGET_Length_MAX"]   >> buff_config_.param.BIG_TARGET_Length_MAX;
  _fs["BIG_TARGET_Length_MIN"]   >> buff_config_.param.BIG_TARGET_Length_MIN;

  _fs["DIFF_ANGLE_MAX"] >> buff_config_.param.DIFF_ANGLE_MAX;
  _fs["DIFF_ANGLE_MIN"] >> buff_config_.param.DIFF_ANGLE_MIN;

  _fs["SMALL_TARGET_ASPECT_RATIO_MAX"] >> buff_config_.param.SMALL_TARGET_ASPECT_RATIO_MAX;
  _fs["SMALL_TARGET_ASPECT_RATIO_MIN"] >> buff_config_.param.SMALL_TARGET_ASPECT_RATIO_MIN;

  _fs["AREA_RATIO_MAX"] >> buff_config_.param.AREA_RATIO_MAX;
  _fs["AREA_RATIO_MIN"] >> buff_config_.param.AREA_RATIO_MIN;
  _fs["BIG_LENTH_R"]    >> buff_config_.param.BIG_LENTH_R;

  _fs["CENTER_R_ROI_SIZE"]  >> buff_config_.param.CENTER_R_ROI_SIZE;
  _fs["FILTER_COEFFICIENT"] >> buff_config_.param.FILTER_COEFFICIENT;

  _fs["BUFF_H"]         >> buff_config_.param.BUFF_H;
  _fs["BUFF_RADIUS"]    >> buff_config_.param.BUFF_RADIUS;
  _fs["PLATFORM_H"]     >> buff_config_.param.PLATFORM_H;
  _fs["BARREL_ROBOT_H"] >> buff_config_.param.BARREL_ROBOT_H;
  _fs["TARGET_X"]       >> buff_config_.param.TARGET_X;

  _fs["OFFSET_FIXED_RADIAN"] >> buff_config_.param.OFFSET_FIXED_RADIAN;
}

void Detector::imageProcessing(cv::Mat&               _input_img,
                               const int&             _my_color,
                               const Processing_Mode& _process_mode) {
  cv::cvtColor(_input_img, gray_img_, cv::COLOR_BGR2GRAY);

  fmt::print("[{}] Image pre-processing mode: {}\n", idntifier_green, _process_mode);

  switch (_process_mode) {
    case BGR_MODE:
      bgrProcessing(_my_color);
      break;
    case HSV_MODE:
      hsvProcessing(_my_color);
      break;
    default: {
      bgrProcessing(_my_color);
      break;
    }
  }

  if (buff_config_.ctrl.IS_SHOW_BIN_IMG == 1 && buff_config_.ctrl.IS_PARAM_ADJUSTMENT == 1) {
    cv::imshow("[basic_buff] imageProcessing() -> bin_img_color_", bin_img_color_);
    cv::imshow("[basic_buff] imageProcessing() -> bin_img_gray_",  bin_img_gray_);
  }

  cv::bitwise_and(bin_img_color_, bin_img_gray_, bin_img_);
  cv::morphologyEx(bin_img_, bin_img_, cv::MORPH_DILATE, ele_);

  if (buff_config_.ctrl.IS_SHOW_BIN_IMG == 1 && buff_config_.ctrl.IS_PARAM_ADJUSTMENT == 1) {
    cv::imshow("[basic_buff] imageProcessing() -> bin_img_", bin_img_);
  }
}

void Detector::bgrProcessing(const int& _my_color) {
  cv::split(src_img_, split_img_);

  switch (_my_color) {
    case uart::RED:
      fmt::print("[{}] Image pre-processing color: RED\n", idntifier_green);

      cv::subtract(split_img_[2], split_img_[0], bin_img_color_);

      if (buff_config_.ctrl.IS_PARAM_ADJUSTMENT == 1) {
        std::string window_name = {"[basic_buff] bgrProcessing() -> trackbar"};

        cv::namedWindow(window_name);
        cv::createTrackbar("GRAY_TH_RED:", window_name, &buff_config_.param.RED_BUFF_GRAY_TH, 255, nullptr);
        cv::createTrackbar("COLOR_TH_RED:", window_name, &buff_config_.param.RED_BUFF_COLOR_TH, 255, nullptr);

        cv::imshow(window_name, trackbar_img_);
      }

      cv::threshold(gray_img_,
                    bin_img_gray_,
                    buff_config_.param.RED_BUFF_GRAY_TH,
                    255,
                    cv::THRESH_BINARY);
      cv::threshold(bin_img_color_,
                    bin_img_color_,
                    buff_config_.param.RED_BUFF_COLOR_TH,
                    255,
                    cv::THRESH_BINARY);

      break;
    case uart::BLUE:
      fmt::print("[{}] Image pre-processing color: BLUE\n", idntifier_green);

      cv::subtract(split_img_[0], split_img_[2], bin_img_color_);  // b-r

      if (buff_config_.ctrl.IS_PARAM_ADJUSTMENT == 1) {
        std::string window_name = {"[basic_buff] bgrProcessing() -> trackbar"};

        cv::namedWindow(window_name);
        cv::createTrackbar("GRAY_TH_BLUE:", window_name, &buff_config_.param.BLUE_BUFF_GRAY_TH, 255, nullptr);
        cv::createTrackbar("COLOR_TH_BLUE:", window_name, &buff_config_.param.BLUE_BUFF_COLOR_TH, 255, nullptr);

        cv::imshow(window_name, trackbar_img_);
      }

      cv::threshold(gray_img_,
                    bin_img_gray_,
                    buff_config_.param.BLUE_BUFF_GRAY_TH,
                    255,
                    cv::THRESH_BINARY);
      cv::threshold(bin_img_color_,
                    bin_img_color_,
                    buff_config_.param.BLUE_BUFF_COLOR_TH,
                    255,
                    cv::THRESH_BINARY);

      break;
    default:
      fmt::print("[{}] Image pre-processing color: default\n", idntifier_green);

      cv::subtract(split_img_[0], split_img_[2], bin_img_color1_);  // b-r
      cv::subtract(split_img_[2], split_img_[0], bin_img_color2_);  // r-b

      if (buff_config_.ctrl.IS_PARAM_ADJUSTMENT == 1) {
        std::string window_name = {"[basic_buff] bgrProcessing() -> trackbar"};

        cv::namedWindow(window_name);
        cv::createTrackbar("GRAY_TH_RED:", window_name, &buff_config_.param.RED_BUFF_GRAY_TH, 255, nullptr);
        cv::createTrackbar("COLOR_TH_RED:", window_name, &buff_config_.param.RED_BUFF_COLOR_TH, 255, nullptr);
        cv::createTrackbar("GRAY_TH_BLUE:", window_name, &buff_config_.param.BLUE_BUFF_GRAY_TH, 255, nullptr);
        cv::createTrackbar("COLOR_TH_BLUE:", window_name, &buff_config_.param.BLUE_BUFF_COLOR_TH, 255, nullptr);

        cv::imshow(window_name, trackbar_img_);
      }

      average_th_ =
        static_cast<int>((buff_config_.param.RED_BUFF_GRAY_TH   +
                          buff_config_.param.BLUE_BUFF_GRAY_TH) *
                          0.5);

      cv::threshold(gray_img_,
                    bin_img_gray_,
                    average_th_,
                    255,
                    cv::THRESH_BINARY);
      cv::threshold(bin_img_color1_,
                    bin_img_color1_,
                    buff_config_.param.BLUE_BUFF_COLOR_TH,
                    255,
                    cv::THRESH_BINARY);
      cv::threshold(bin_img_color2_,
                    bin_img_color2_,
                    buff_config_.param.RED_BUFF_COLOR_TH,
                    255,
                    cv::THRESH_BINARY);

      cv::bitwise_or(bin_img_color1_, bin_img_color2_, bin_img_color_);

      break;
  }

  split_img_.clear();
  std::vector<cv::Mat>(split_img_).swap(split_img_);
}

void Detector::hsvProcessing(const int& _my_color) {
  cv::cvtColor(src_img_, hsv_img_, cv::COLOR_BGR2HSV_FULL);

  switch (_my_color) {
    case uart::RED:
      fmt::print("[{}] Image pre-processing color: RED\n", idntifier_green);

      if (buff_config_.ctrl.IS_PARAM_ADJUSTMENT == 1) {
        std::string window_name = {"[basic_buff] hsvProcessing() -> trackbar"};

        cv::namedWindow(window_name);
        cv::createTrackbar("GRAY_TH_RED:", window_name, &buff_config_.param.RED_BUFF_GRAY_TH, 255, nullptr);
        cv::createTrackbar("H_RED_MAX:", window_name, &buff_config_.param.H_RED_MAX, 360, nullptr);
        cv::createTrackbar("H_RED_MIN:", window_name, &buff_config_.param.H_RED_MIN, 360, nullptr);
        cv::createTrackbar("S_RED_MAX:", window_name, &buff_config_.param.S_RED_MAX, 255, nullptr);
        cv::createTrackbar("S_RED_MIN:", window_name, &buff_config_.param.S_RED_MIN, 255, nullptr);
        cv::createTrackbar("V_RED_MAX:", window_name, &buff_config_.param.V_RED_MAX, 255, nullptr);
        cv::createTrackbar("V_RED_MIN:", window_name, &buff_config_.param.V_RED_MIN, 255, nullptr);

        imshow(window_name, trackbar_img_);
      }

      cv::inRange(hsv_img_,
                  cv::Scalar(buff_config_.param.H_RED_MIN,
                             buff_config_.param.S_RED_MIN,
                             buff_config_.param.V_RED_MIN),
                  cv::Scalar(buff_config_.param.H_RED_MAX,
                             buff_config_.param.S_RED_MAX,
                             buff_config_.param.V_RED_MAX),
                  bin_img_color_);

      cv::threshold(gray_img_,
                   bin_img_gray_,
                   buff_config_.param.RED_BUFF_GRAY_TH,
                   255,
                   cv::THRESH_BINARY);

      break;
    case uart::BLUE:
      fmt::print("[{}] Image pre-processing color: BLUE\n", idntifier_green);

      if (buff_config_.ctrl.IS_PARAM_ADJUSTMENT == 1) {
        std::string window_name = {"[basic_buff] hsvProcessing() -> trackbar"};

        cv::namedWindow(window_name);
        cv::createTrackbar("GRAY_TH_BLUE:", window_name, &buff_config_.param.BLUE_BUFF_GRAY_TH, 255, nullptr);
        cv::createTrackbar("H_BLUE_MAX:", window_name, &buff_config_.param.H_BLUE_MAX, 255, nullptr);
        cv::createTrackbar("H_BLUE_MIN:", window_name, &buff_config_.param.H_BLUE_MIN, 255, nullptr);
        cv::createTrackbar("S_BLUE_MAX:", window_name, &buff_config_.param.S_BLUE_MAX, 255, nullptr);
        cv::createTrackbar("S_BLUE_MIN:", window_name, &buff_config_.param.S_BLUE_MIN, 255, nullptr);
        cv::createTrackbar("V_BLUE_MAX:", window_name, &buff_config_.param.V_BLUE_MAX, 255, nullptr);
        cv::createTrackbar("V_BLUE_MIN:", window_name, &buff_config_.param.V_BLUE_MIN, 255, nullptr);

        cv::imshow(window_name, trackbar_img_);
      }

      cv::inRange(hsv_img_,
                  cv::Scalar(buff_config_.param.H_BLUE_MIN,
                             buff_config_.param.S_BLUE_MIN,
                             buff_config_.param.V_BLUE_MIN),
                  cv::Scalar(buff_config_.param.H_BLUE_MAX,
                             buff_config_.param.S_BLUE_MAX,
                             buff_config_.param.V_BLUE_MAX),
                  bin_img_color_);

      cv::threshold(gray_img_,
                    bin_img_gray_,
                    buff_config_.param.BLUE_BUFF_GRAY_TH,
                    255,
                    cv::THRESH_BINARY);

      break;
    default:
      fmt::print("[{}] Image pre-processing color: default\n", idntifier_green);

      if (buff_config_.ctrl.IS_PARAM_ADJUSTMENT == 1) {
        std::string window_name = {"[basic_buff] hsvProcessing() -> trackbar"};

        cv::namedWindow(window_name);
        cv::createTrackbar("GRAY_TH_RED:", window_name, &buff_config_.param.RED_BUFF_GRAY_TH, 255, nullptr);
        cv::createTrackbar("H_RED_MAX:", window_name, &buff_config_.param.H_RED_MAX, 360, nullptr);
        cv::createTrackbar("H_RED_MIN:", window_name, &buff_config_.param.H_RED_MIN, 360, nullptr);
        cv::createTrackbar("S_RED_MAX:", window_name, &buff_config_.param.S_RED_MAX, 255, nullptr);
        cv::createTrackbar("S_RED_MIN:", window_name, &buff_config_.param.S_RED_MIN, 255, nullptr);
        cv::createTrackbar("V_RED_MAX:", window_name, &buff_config_.param.V_RED_MAX, 255, nullptr);
        cv::createTrackbar("V_RED_MIN:", window_name, &buff_config_.param.V_RED_MIN, 255, nullptr);

        cv::createTrackbar("GRAY_TH_BLUE:", window_name, &buff_config_.param.BLUE_BUFF_GRAY_TH, 255, nullptr);
        cv::createTrackbar("H_BLUE_MAX:", window_name, &buff_config_.param.H_BLUE_MAX, 255, nullptr);
        cv::createTrackbar("H_BLUE_MIN:", window_name, &buff_config_.param.H_BLUE_MIN, 255, nullptr);
        cv::createTrackbar("S_BLUE_MAX:", window_name, &buff_config_.param.S_BLUE_MAX, 255, nullptr);
        cv::createTrackbar("S_BLUE_MIN:", window_name, &buff_config_.param.S_BLUE_MIN, 255, nullptr);
        cv::createTrackbar("V_BLUE_MAX:", window_name, &buff_config_.param.V_BLUE_MAX, 255, nullptr);
        cv::createTrackbar("V_BLUE_MIN:", window_name, &buff_config_.param.V_BLUE_MIN, 255, nullptr);

        imshow(window_name, trackbar_img_);
      }

      average_th_ =
        static_cast<int>((buff_config_.param.RED_BUFF_GRAY_TH   +
                          buff_config_.param.BLUE_BUFF_GRAY_TH) *
                          0.5);

      cv::threshold(gray_img_,
                    bin_img_gray_,
                    average_th_,
                    255,
                    cv::THRESH_BINARY);

      cv::inRange(hsv_img_,
                  cv::Scalar(buff_config_.param.H_RED_MIN,
                             buff_config_.param.S_RED_MIN,
                             buff_config_.param.V_RED_MIN),
                  cv::Scalar(buff_config_.param.H_RED_MAX,
                             buff_config_.param.S_RED_MAX,
                             buff_config_.param.V_RED_MAX),
                  bin_img_color2_);
      cv::inRange(hsv_img_,
                  cv::Scalar(buff_config_.param.H_BLUE_MIN,
                             buff_config_.param.S_BLUE_MIN,
                             buff_config_.param.V_BLUE_MIN),
                  cv::Scalar(buff_config_.param.H_BLUE_MAX,
                             buff_config_.param.S_BLUE_MAX,
                             buff_config_.param.V_BLUE_MAX),
                  bin_img_color1_);

      cv::bitwise_or(bin_img_color1_, bin_img_color2_, bin_img_color_);

      break;
  }
}

void Detector::findTarget(cv::Mat&                              _input_dst_img,
                          cv::Mat&                              _input_bin_img,
                          std::vector<abstract_target::Target>& _target_box) {
  cv::findContours(_input_bin_img, contours_, hierarchy_, 2, cv::CHAIN_APPROX_NONE);

  for (size_t i = 0; i != contours_.size(); ++i) {
    if (hierarchy_[i][3]    < 0 ||
        contours_[i].size() < 6 ||
        contours_[static_cast<uint>(hierarchy_[i][3])].size() < 6) {
      continue;
    }

    small_rect_length_ = arcLength(contours_[i], true);
    if (small_rect_length_ < buff_config_.param.SMALL_TARGET_Length_MIN) {
      continue;
    }

    small_rect_area_ = contourArea(contours_[i]);
    if (small_rect_area_ < buff_config_.param.SMALL_TARGET_AREA_MIN ||
        small_rect_area_ > buff_config_.param.SMALL_TARGET_AREA_MAX) {
      continue;
    }

    big_rect_length_ = arcLength(contours_[static_cast<uint>(hierarchy_[i][3])], true);
    if (big_rect_length_ < buff_config_.param.BIG_TARGET_Length_MIN) {
      continue;
    }

    big_rect_area_ = contourArea(contours_[static_cast<uint>(hierarchy_[i][3])]);
    if (big_rect_area_ < buff_config_.param.BIG_TARGET_AREA_MIN ||
        big_rect_area_ > buff_config_.param.BIG_TARGET_AREA_MAX) {
      continue;
    }

    small_target_.inputParams(contours_[i]);
    big_target_.inputParams(contours_[static_cast<uint>(hierarchy_[i][3])]);
    candidated_target_.inputParams(big_target_, small_target_);
    big_target_.displayFanBlade(_input_dst_img);

    if (candidated_target_.diffAngle() >= buff_config_.param.DIFF_ANGLE_MAX ||
        candidated_target_.diffAngle() <= buff_config_.param.DIFF_ANGLE_MIN) {
      continue;
    }

    if (candidated_target_.getArmor().aspectRatio() >=
        buff_config_.param.SMALL_TARGET_ASPECT_RATIO_MAX ||
        candidated_target_.getArmor().aspectRatio() <=
        buff_config_.param.SMALL_TARGET_ASPECT_RATIO_MIN) {
      continue;
    }

    if (candidated_target_.areaRatio() <= buff_config_.param.AREA_RATIO_MIN ||
        candidated_target_.areaRatio() >= buff_config_.param.AREA_RATIO_MAX) {
      continue;
    }

    small_target_.displayFanArmor(_input_dst_img);
    big_target_.displayFanBlade(_input_dst_img);

    candidated_target_.setType(abstract_object::ACTION);
    candidated_target_.updateVertex(_input_dst_img);
    candidated_target_.setType(_input_bin_img);

    _target_box.emplace_back(candidated_target_);
  }

  fmt::print("[{}] Number of fans: {}\n", idntifier_green, _target_box.size());
}

bool Detector::isFindTarget(cv::Mat& _input_img, std::vector<abstract_target::Target>& _target_box) {
  if (_target_box.size() < 1) {
    fmt::print("[{}] Info, no target detected\n", idntifier_green);

    current_target_ = abstract_target::Target();

    contours_.clear();
    hierarchy_.clear();
    _target_box.clear();

    std::vector<std::vector<cv::Point>>(contours_).swap(contours_);
    std::vector<cv::Vec4i>(hierarchy_).swap(hierarchy_);
    std::vector<abstract_target::Target>(_target_box).swap(_target_box);

    return false;
  }

  action_cnt_   = 0;
  inaction_cnt_ = 0;

  for (auto iter = _target_box.begin(); iter != _target_box.end(); ++iter) {
    if (iter->getType() != abstract_object::INACTION) {
      ++action_cnt_;
      continue;
    }

    ++inaction_cnt_;

    current_target_ = *iter;
    current_target_.displayInactionTarget(_input_img);
  }

  fmt::print("[{}] Number of hits/unhits: {}, {}\n", idntifier_green, inaction_cnt_, action_cnt_);

  contours_.clear();
  hierarchy_.clear();
  _target_box.clear();

  std::vector<std::vector<cv::Point>>(contours_).swap(contours_);
  std::vector<cv::Vec4i>(hierarchy_).swap(hierarchy_);
  std::vector<abstract_target::Target>(_target_box).swap(_target_box);

  return inaction_cnt_ > 0;
}

cv::Point2f Detector::findCircleR(cv::Mat&    _input_src_img,
                                  cv::Mat&    _input_bin_img,
                                  cv::Mat&    _dst_img,
                                  const bool& _is_find_target) {
  _input_src_img.copyTo(roi_img_);
  _input_bin_img.copyTo(result_img_);

  cv::Point2f center_r_point2f = cv::Point2f(0.f, 0.f);

  if (!_is_find_target) {
    is_circle_        = false;
    roi_local_center_ = cv::Point2f(0.f, 0.f);

    center_r_box_.clear();
    std::vector<abstract_center_r::Center_R>(center_r_box_).swap(center_r_box_);

    return center_r_point2f;
  }

  delta_height_point_ = current_target_.deltaPoint();
  roi_global_center_  = current_target_.getArmor().getRect().center -
                        buff_config_.param.BIG_LENTH_R              *
                        delta_height_point_;

  if (roi_global_center_.x < 0                   ||
      roi_global_center_.y < 0                   ||
      roi_global_center_.x > _input_src_img.cols ||
      roi_global_center_.y > _input_src_img.rows) {
    if (roi_global_center_.x < 0) {
      roi_global_center_.x = 1;
    }
    if (roi_global_center_.y < 0) {
      roi_global_center_.y = 1;
    }
    if (roi_global_center_.x > _input_src_img.cols) {
      roi_global_center_.x = _input_src_img.cols - 1;
    }
    if (roi_global_center_.y > _input_src_img.rows - 1) {
      roi_global_center_.y = _input_src_img.rows - 1;
    }
  }

  cv::RotatedRect roi_R(roi_global_center_,
                        cv::Size(buff_config_.param.CENTER_R_ROI_SIZE,
                                 buff_config_.param.CENTER_R_ROI_SIZE),
                        0);
  cv::Rect        roi = roi_R.boundingRect();

  roi         = roi_tool_.makeRectSafeTailor(_input_src_img, roi);
  result_img_ = roi_tool_.cutROIRect(_input_bin_img, roi);
  roi_img_    = roi_tool_.cutROIRect(_input_src_img, roi);

  cv::rectangle(_dst_img, roi, cv::Scalar(0, 255, 200), 2, 8, 0);

  is_circle_        = false;
  roi_local_center_ = cv::Point2f(roi_img_.cols * 0.5, roi_img_.rows * 0.5);

  cv::findContours(result_img_, contours_r_, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);

  fmt::print("[{}] Rectangular proportion/area:", idntifier_green);

  for (size_t i = 0; i != contours_r_.size(); ++i) {
    if (contours_r_[i].size() < 6) {
      continue;
    }

    center_r_.inputParams(contours_r_[i], roi_img_);

    if (center_r_.aspectRatio() < 0.9f || center_r_.aspectRatio() > 1.25f) {
      continue;
    }

    if (center_r_.getRect().boundingRect().area() < 1000 ||
        center_r_.getRect().boundingRect().area() > 3500) {
      continue;
    }

    fmt::print(" [{}]{}/{},",
               i,
               center_r_.aspectRatio(),
               center_r_.getRect().boundingRect().area());

    center_r_box_.emplace_back(center_r_);

    for (size_t j = 0; j != 4; ++j) {
      cv::line(roi_img_,
               center_r_.getVertex(j),
               center_r_.getVertex((j + 1) % 4),
               cv::Scalar(0, 130, 255),
               3);
    }

    fmt::print("\n");
  }

  fmt::print("[{}] Eligible rectangular(s) for the ratio: {}\n",
             idntifier_green,
             center_r_box_.size());

  if (center_r_box_.size() < 1) {
    is_circle_       = false;
    center_r_point2f = roi_global_center_;

    cv::line(_dst_img,
             current_target_.getArmor().getRect().center,
             center_r_point2f,
             cv::Scalar(0, 0, 255),
             2);
    cv::circle(_dst_img, center_r_point2f, 2, cv::Scalar(0, 0, 255), 2, 8, 0);
  } else {
    std::sort(center_r_box_.begin(), center_r_box_.end(),
      [](abstract_center_r::Center_R& c1, abstract_center_r::Center_R& c2) {
        return c1.centerDist() < c2.centerDist();
      });

    is_circle_       = true;
    center_r_point2f = center_r_box_[0].getRect().center + roi_R.boundingRect2f().tl();

    cv::line(_dst_img,
             current_target_.getArmor().getRect().center,
             center_r_point2f,
             cv::Scalar(0, 255, 0),
             2);
    cv::circle(_dst_img, center_r_point2f, 2, cv::Scalar(0, 0, 255), 2, 8, 0);
  }

  center_r_box_.clear();
  contours_r_.clear();

  std::vector<abstract_center_r::Center_R>(center_r_box_).swap(center_r_box_);
  std::vector<std::vector<cv::Point>>(contours_r_).swap(contours_r_);

  return center_r_point2f;
}

void Detector::judgeCondition(const bool& _is_find_target) {
  if (!_is_find_target) {
    current_angle_ = last_target_.getAngle();
    diff_angle_    = 0.f;
    current_speed_ = 0.f;

    return;
  }

  calAngle();
  calDirection();
  calVelocity();

  return;
}

void Detector::calAngle() {
  current_angle_ =
    atan2((current_target_.getArmor().getRect().center.y - final_center_r_.y),
          (current_target_.getArmor().getRect().center.x - final_center_r_.x)) *
    180                                                                        /
    static_cast<float>(CV_PI);

  if (current_angle_ < 0.f) {
    current_angle_ += 360.f;
  }

  diff_angle_ = current_angle_ - last_angle_;

  if (diff_angle_ > 180) {
    diff_angle_ -= 360;
  } else if (diff_angle_ < -180) {
    diff_angle_ += 360;
  }

  fmt::print("[{}] Current angle difference: {}\n", idntifier_green, diff_angle_);

  if (fabs(diff_angle_) > 30.f) {
    is_change_blade_ = true;
    diff_angle_      = 0.f;
  } else {
    is_change_blade_ = false;
  }
}

void Detector::calDirection() {
  ++find_cnt_;

  if (find_cnt_ % 2 == 0) {
    current_direction_ = getState();
    filter_direction_  = (1 - 0.01) * last_direction_ + 0.01 * current_direction_;
    last_direction_    = filter_direction_;
  }

  if (find_cnt_ == 10) {
    find_cnt_ = 0;
  }

  if (filter_direction_ > 0.1) {
    fmt::print("[{}] Turning direction: clockwise\n", idntifier_green);

    final_direction_      = 1;
    last_final_direction_ = final_direction_;
  } else if (filter_direction_ < -0.1) {
    fmt::print("[{}] Turning direction: counter-clockwise\n", idntifier_green);

    final_direction_      = -1;
    last_final_direction_ = final_direction_;
  } else {
    fmt::print("[{}] Turning direction: stop\n", idntifier_green);

    final_direction_ = last_final_direction_;
  }
}

int Detector::getState() {
  if (fabs(diff_angle_) < 10 &&
      fabs(diff_angle_) > 1e-6) {
    d_angle_ = (1 - buff_config_.param.FILTER_COEFFICIENT) *
               d_angle_                                    +
               buff_config_.param.FILTER_COEFFICIENT       *
               diff_angle_;
  }

  if (d_angle_ > 0) {
    return 1;
  } else if (d_angle_ < -0) {
    return -1;
  } else {
    return 0;
  }
}

void Detector::calVelocity() {
  double current_time       = buff_fps_.lastTime();
  float  current_diff_angle = diff_angle_;

  if (find_cnt_ % 2 == 0) {
    current_time       += last_time_;
    current_diff_angle += last_diff_angle_;

    if (current_time == 0) {
      current_speed_ = 0.f;
    } else {
      current_speed_ = fabs(current_diff_angle / current_time * CV_PI / 180);
    }
  } else {
    last_time_       = current_time;
    last_diff_angle_ = diff_angle_;
  }

  fmt::print("[{}] The current rotate speed is: {}\n", idntifier_green, current_speed_);
}

float Detector::doPredict(const float& _bullet_velocity[[maybe_unused]],
                          const bool&  _is_find_target) {
  if (!_is_find_target) {
    target_z_ = 0.f;

    return 0.f;
  }

  float predict_quantity = 0.f;

  predict_quantity = fixedPredict(28 * 1000);

  return predict_quantity;
}

float Detector::fixedPredict(const float& _bullet_velocity) {
  current_radian_ = current_angle_ * CV_PI / 180;
  target_buff_h_  = 800 + sin(current_radian_ - CV_PI) * 800;

  target_y_       = target_buff_h_ + barrel_buff_botton_h_;
  target_x_       = buff_config_.param.TARGET_X;
  target_z_       = sqrt((target_y_ * target_y_) + (target_x_ * target_x_));

  bullet_tof_     = (target_z_ + offset_angle_float_) / _bullet_velocity;

  if (current_direction_ != 0) {
    fixed_forecast_quantity_ = current_speed_ *
                               bullet_tof_    +
                               buff_config_.param.OFFSET_FIXED_RADIAN;

  } else {
    fixed_forecast_quantity_ = 0.f;
  }

  return fixed_forecast_quantity_;
}

void Detector::calculateTargetPointSet(const float&              _predict_quantity,
                                       const cv::Point2f&        _final_center_r,
                                       std::vector<cv::Point2f>& _target_2d_point,
                                       cv::Mat&                  _input_dst_img,
                                       const bool&               _is_find_target) {
  if (!_is_find_target) {
    _target_2d_point.clear();
    _target_2d_point = std::vector<cv::Point2f>(4, cv::Point2f(0.f, 0.f));

    return;
  }

  theta_ = current_radian_;

  if (theta_ < 0) {
    theta_ += (2 * CV_PI);
  }

  final_radian_ = theta_ + final_direction_ * _predict_quantity;
  final_angle_  = final_radian_ * 180 / CV_PI;

  sin_calcu_ = sin(final_radian_);
  cos_calcu_ = cos(final_radian_);

  radio_     =
    abstract_object::centerDistance(_final_center_r,
                                    current_target_.getArmor().getRect().center);

  pre_center_.x = radio_ * cos_calcu_ + _final_center_r.x;
  pre_center_.y = radio_ * sin_calcu_ + _final_center_r.y;

  target_rect_ = cv::RotatedRect(pre_center_, current_target_.getArmor().getRect().size, 90);

  target_buff_h_ = 800 + sin(final_radian_ - CV_PI) * 800;
  target_y_      = target_buff_h_ + barrel_buff_botton_h_;

  final_target_z_ = sqrt((target_y_ * target_y_) + (target_x_ * target_x_));

  _target_2d_point.clear();

  _target_2d_point.emplace_back(current_target_.vector2DPoint(0));
  _target_2d_point.emplace_back(current_target_.vector2DPoint(1));
  _target_2d_point.emplace_back(current_target_.vector2DPoint(2));
  _target_2d_point.emplace_back(current_target_.vector2DPoint(3));

  for (size_t i = 0; i != 4; ++i) {
    cv::line(_input_dst_img,
             _target_2d_point[i],
             _target_2d_point[(i + 1) % 4],
             cv::Scalar(0, 130, 255),
             8);
  }

  cv::circle(_input_dst_img, _final_center_r, radio_, cv::Scalar(0, 255, 125), 2, 8, 0);
  cv::circle(_input_dst_img, pre_center_, 3, cv::Scalar(255, 0, 0), 3, 8, 0);

  cv::line(_input_dst_img, pre_center_, _final_center_r, cv::Scalar(0, 255, 255), 2);
  cv::line(_input_dst_img, current_target_.getArmor().getRect().center, _final_center_r, cv::Scalar(0, 255, 0), 2);

  cv::circle(_input_dst_img, _target_2d_point[0], 10, cv::Scalar(0, 0, 255), -1, 8, 0);
  cv::circle(_input_dst_img, _target_2d_point[1], 10, cv::Scalar(0, 255, 255), -1, 8, 0);
  cv::circle(_input_dst_img, _target_2d_point[2], 10, cv::Scalar(255, 0, 0), -1, 8, 0);
  cv::circle(_input_dst_img, _target_2d_point[3], 10, cv::Scalar(0, 255, 0), -1, 8, 0);
}

void Detector::updateLastData(const bool& _is_find_target) {
  if (!_is_find_target) {
    is_find_last_target_ = _is_find_target;

    target_2d_point_.clear();

    std::vector<cv::Point2f>(target_2d_point_).swap(target_2d_point_);
    target_rect_ = cv::RotatedRect();

    return;
  }

  last_target_         = current_target_;
  last_angle_          = current_angle_;
  is_find_last_target_ = _is_find_target;

  target_2d_point_.clear();

  std::vector<cv::Point2f>(target_2d_point_).swap(target_2d_point_);
  target_rect_ = cv::RotatedRect();
}

}  // namespace basic_buff
