#include <iostream>
#include <future>
#include <stdio.h>

#include <QThread>
#include <QEventLoop>
#include <QCoreApplication>
#include <QVideoFrame>

#include "qtio/qt_io.hpp"

qt_video_player::qt_video_player(std::string uri, int32_t width, int32_t height) 
                                  : m_camera(nullptr),
                                    m_capture(nullptr),
                                    m_player(nullptr),
                                    m_specified_url(uri),
                                    m_surface_for_player(nullptr),
                                    m_width(width),
                                    m_height(height),
                                    m_mutex{}
{
    start();
}

void stateChanged(QMediaPlayer::PlaybackState newState)
{
  std::cout << "Playback state changed: " << newState << std::endl;
}

void playerErrorOccurred(QMediaPlayer::Error error, const QString &errorString)
{
  if (error != QMediaPlayer::Error::NoError)
  {
    std::cout << "ERROR: " << errorString.toStdString() << std::endl;
  }
}

void cameraErrorOccurred(QCamera::Error error) ///, const QString &errorString)
{
  if (error != QCamera::Error::NoError)
  {
    std::cout << "ERROR: "  << std::endl;
  }
}

void qt_video_player::frame_changed(const QVideoFrame &frame)
{
  const QImage &imageSmall = frame.toImage();

  QImage image;
  if (m_width > 0 && m_height > 0)
  {
    image = imageSmall.scaled(m_width, m_height,
                              Qt::IgnoreAspectRatio,
                              Qt::SmoothTransformation);
  }
  else
  {
    image = imageSmall;
  }
  
  std::lock_guard<std::mutex> lock(m_mutex);

  int64_t height = image.height();
  int64_t width = image.width();
  uint8_t rgb = 3;
  
  QImage image2 = image.convertToFormat(QImage::Format_RGB888);
  uint8_t *bits = image2.bits();

  DLTensor *output_tensor = new DLTensor;
  output_tensor->device.device_type = DLDeviceType::kDLCPU;
  output_tensor->ndim = 3;
  output_tensor->shape = new int64_t[]{rgb, height, width};
  output_tensor->strides = NULL;
  output_tensor->byte_offset = 0;
  output_tensor->dtype.code = DLDataTypeCode::kDLUInt;
  output_tensor->dtype.bits = 8;
  output_tensor->dtype.lanes = 3;
  output_tensor->data = new uint8_t[width * height * rgb];
  memcpy(output_tensor->data, bits, width * height * rgb * sizeof(uint8_t));
  
  m_frame_queue.push(output_tensor);
  m_condition.notify_one();
}

void mediaStatusChanged(QMediaPlayer::MediaStatus status)
{
  std::cout << "Media status changed: " << status << std::endl;
}

void qt_video_player::run()
{
  if(m_specified_url == "camera")
    m_source_type = CAMERA;
  else
    m_source_type = VIDEO;

  switch(m_source_type) {
  case CAMERA:
    {
      m_camera = new QCamera(QCameraDevice::UnspecifiedPosition);
      const auto settings = m_camera->cameraDevice().videoFormats();
      auto s = settings.at( 1 );
      m_camera->setCameraFormat( s );

      m_capture = new QMediaCaptureSession;
      m_surface_for_player = new QVideoSink(m_capture);
      
      QObject::connect(m_camera, &QCamera::errorOccurred, cameraErrorOccurred);
      m_capture->setCamera( m_camera );
      m_camera->setExposureMode(QCamera::ExposureMode::ExposureAuto);
      m_capture->setVideoSink( m_surface_for_player );

      QObject::connect(m_surface_for_player, &QVideoSink::videoFrameChanged,
                                          [this](const QVideoFrame &frame) {
                                            this->frame_changed(frame);
                                          });
      m_camera->start();
    }
    break;
  case VIDEO:
    m_player = new QMediaPlayer;

    QObject::connect(m_player, &QMediaPlayer::playbackStateChanged,
                    stateChanged);
    QObject::connect(m_player, &QMediaPlayer::mediaStatusChanged,
                    mediaStatusChanged);
    QObject::connect(m_player, &QMediaPlayer::errorOccurred,
                    playerErrorOccurred);

    QUrl resource = QUrl::fromLocalFile((m_specified_url).c_str());

    m_player->setSource(resource);

    m_surface_for_player = new QVideoSink(m_player);
    m_player->setVideoSink(m_surface_for_player);

    QObject::connect(m_surface_for_player, &QVideoSink::videoFrameChanged,
                    [this](const QVideoFrame &frame)
                    {
                      this->frame_changed(frame);
                    });
    m_player->play();
  }
  exec();
}

qt_video_player::~qt_video_player()
{
  if (m_player != nullptr)
  {
    delete m_player;
    m_player = nullptr;
  }

  if (m_camera != nullptr)
  {
    delete m_camera;
    m_camera = nullptr;
  }
}

DLTensor *qt_video_player::update()
{
  std::unique_lock<std::mutex> lock(m_mutex);
  
  while (m_frame_queue.empty())
  {
    // release lock as long as the wait and reaquire it afterwards.
    m_condition.wait(lock);
  }
  DLTensor *val = m_frame_queue.front();
  m_frame_queue.pop();
  
  return val;
}

#ifdef __clang__
#pragma GCC diagnostic ignored "-Wreturn-type-c-linkage"
#endif

#define DL_EXPORT __attribute__((visibility("default")))

extern "C" DL_EXPORT std::string get_simple_description()
{
  return std::string("This library will load video files with QT multimedia and push it through the DAG.");
}

extern "C" DL_EXPORT std::string get_detailed_description()
{
  return std::string("This library will load video files with QT multimedia and push it through the DAG. DETAILED");
}

extern "C" DL_EXPORT std::string get_name()
{
  return std::string("QT Multimedia input");
}

extern "C" DL_EXPORT long get_serial_guid()
{
  return 24680;
}

extern "C" DL_EXPORT bool is_source()
{
  return true;
}

extern "C" DL_EXPORT fn_dag::lib_options get_options()
{
  fn_dag::lib_options options;
  fn_dag::construction_option optionFilePath{fn_dag::STRING, {""}, 9185, "Local location of file", "Specify the file location of the video source."};
  fn_dag::construction_option optionWidth{fn_dag::INT, {"640"}, 9011, "Width of output image", "Specify the width of the output image in pixels. Set either of these to zero to keep the source resolution."};
  fn_dag::construction_option optionHeight{fn_dag::INT, {"480"}, 9012, "Height of output image", "Specify the height of the output image in pixels. Set either of these to zero to keep the source resolution."};

  options.push_back(optionFilePath);
  options.push_back(optionWidth);
  options.push_back(optionHeight);
  return options;
}

extern "C" DL_EXPORT fn_dag::module *get_module(const fn_dag::lib_options *options)
{
  if (options->size() != 3)
    return nullptr;

  std::string file_path;
  int32_t width = 0;
  int32_t height = 0;

  for (auto option : *options)
  {
    switch (option.serial_id)
    {
    case 9185:
      if (option.type == fn_dag::OPTION_TYPE::STRING)
        file_path = option.value.string_value;
      break;
    case 9011:
      width = option.value.int_value;
      break;
    case 9012:
      height = option.value.int_value;
      break;
    }
  }

  fn_dag::source_handler *vlc_out = new fn_dag::source_handler(new qt_video_player(file_path, width, height));
  return (fn_dag::module *)vlc_out;
}