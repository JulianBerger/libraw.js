/*
 * libraw.js - node wrapper for LibRaw
 * Copyright (C) 2020-2021  Justin Kambic
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 *
 * Direct further questions to justinkambic.github@gmail.com.
 */

#include <napi.h>
#include "libraw_wrapper.h"
#include "wraptypes.h"
#include <fstream>

Napi::Object LibRawWrapper::Init(Napi::Env &env, Napi::Object &exports)
{
  Napi::HandleScope scope(env);

  Napi::Function func =
      DefineClass(
          env,
          "LibRawWrapper",
          {InstanceMethod("getMetadata", &LibRawWrapper::GetMetadata),
           InstanceMethod("getThumbnail", &LibRawWrapper::GetThumbnail),
           InstanceMethod("getXmp", &LibRawWrapper::GetXmpData),
           InstanceMethod("cameraCount", &LibRawWrapper::CameraCount),
           InstanceMethod("cameraList", &LibRawWrapper::CameraList),
           InstanceMethod("open_file", &LibRawWrapper::OpenFile),
           InstanceMethod("open_buffer", &LibRawWrapper::OpenBuffer),
           InstanceMethod("unpack", &LibRawWrapper::Unpack),
           InstanceMethod("unpack_thumb", &LibRawWrapper::UnpackThumb),
           InstanceMethod("extract_tiff", &LibRawWrapper::ExtractTiff),
           InstanceMethod("recycle", &LibRawWrapper::Recycle),
           InstanceMethod("error_count", &LibRawWrapper::ErrorCount),
           InstanceMethod("recycle_datastream", &LibRawWrapper::RecycleDatastream),
           InstanceMethod("strerror", &LibRawWrapper::StrError),
           InstanceMethod("version", &LibRawWrapper::Version),
           InstanceMethod("versionNumber", &LibRawWrapper::VersionNumber)});

  exports.Set("LibRawWrapper", func);
  return exports;
}

LibRawWrapper::LibRawWrapper(const Napi::CallbackInfo &info) : Napi::ObjectWrap<LibRawWrapper>(info)
{
  this->processor_ = new LibRaw();
}

Napi::Value LibRawWrapper::GetThumbnail(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  if (this->processor_->imgdata.thumbnail.thumb)
  {
    return Napi::Buffer<char>::New(
        env,
        this->processor_->imgdata.thumbnail.thumb,
        this->processor_->imgdata.thumbnail.tlength);
  }

  Napi::Error::New(
      env,
      "Thumbnail is not unpacked or is null.")
      .ThrowAsJavaScriptException();

  return env.Undefined();
}

Napi::Value LibRawWrapper::GetXmpData(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();

  char *xmp = this->processor_->imgdata.idata.xmpdata;
  if (xmp)
  {
    return Napi::Buffer<char>::New(
        env,
        xmp,
        this->processor_->imgdata.idata.xmplen);
  }
  return Napi::Object::New(env);
}

Napi::Value LibRawWrapper::GetMetadata(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  libraw_data_t data = this->processor_->imgdata;
  return WrapLibRawData(&env, &data);
}

Napi::Value LibRawWrapper::OpenFile(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  if (!info[0].IsString())
  {
    Napi::TypeError::New(env, "openFile received an invalid argument, filename must be a string.").ThrowAsJavaScriptException();
  }
  if (info.Length() == 2 && !info[1].IsNumber())
  {
    Napi::TypeError::New(env, "openFile received an invalid argument, bigfile_size must be a number.").ThrowAsJavaScriptException();
  }
  Napi::String filename = info[0].As<Napi::String>();
  int ret = this->processor_->open_file(filename.Utf8Value().c_str());

  return Napi::Value::From(env, ret);
}

Napi::Value LibRawWrapper::OpenBuffer(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  if (info.Length() != 1 || !info[0].IsBuffer())
  {
    Napi::TypeError::New(env, "openBuffer received a null argument, buffer is required.").ThrowAsJavaScriptException();
  }
  Napi::Buffer<char> buffer = info[0].As<Napi::Buffer<char>>();
  return Napi::Value::From(
      env,
      this->processor_->open_buffer(buffer.Data(), buffer.Length()));
}

Napi::Value LibRawWrapper::Unpack(const Napi::CallbackInfo &info)
{
  return Napi::Value::From(
      info.Env(),
      this->processor_->unpack());
}

Napi::Value LibRawWrapper::UnpackThumb(const Napi::CallbackInfo &info)
{
  return Napi::Value::From(
      info.Env(),
      this->processor_->unpack_thumb());
}

Napi::Value LibRawWrapper::ExtractTiff(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  if (!info[0].IsString())
  {
    Napi::Error::New(env, "extract_tiff received an invalid argument, tiffpath must be a string.").ThrowAsJavaScriptException();
  }

  Napi::String filename = info[0].As<Napi::String>();

  this->processor_->imgdata.params.output_tiff = 1;        // output as a tiff!
  this->processor_->imgdata.params.output_bps = 8;         // 8 bits export is enough
  this->processor_->imgdata.params.output_color = 1;       // sRGB = 1
  this->processor_->imgdata.params.use_camera_wb = 1;      // use camera white balance
  this->processor_->imgdata.params.use_camera_matrix = 1;  // use camera color matrix
  this->processor_->imgdata.params.no_auto_bright = 0;     // contrast stretch the image
  this->processor_->imgdata.params.auto_bright_thr = 0.01; // contrast stretch the image

  this->processor_->imgdata.params.highlight = 0; // Highlights processing
  // this->processor_->imgdata.params.med_passes = 1;        // median filter passes
  // this->processor_->imgdata.params.fbdd_noiserd = 1; // noise reduction

  this->processor_->imgdata.params.gamm[0] = 1.0 / 2.222f; // rec. BT.709
  this->processor_->imgdata.params.gamm[1] = 4.5f;         // rec. BT.709

  /* exposure correction (lighten the raw while preserving the highlights) */
  this->processor_->imgdata.params.exp_correc = 1;
  this->processor_->imgdata.params.exp_shift = 2.25f; /* 0.25 = 2stop dark, 8 = 3 stop light */
  this->processor_->imgdata.params.exp_preser = 1.0f; /* 0 - 1.0 */

  /* user_qual 0 - linear interpolation
             1 - VNG interpolation
             2 - PPG interpolation <--- Best qual/perf
             3 - AHD interpolation <--- Best qual
             4 - DCB interpolation
            11 - DHT interpolation <--- Best performance 2x slower
*/
  this->processor_->imgdata.params.user_qual = 2; // interpolation

  this->processor_->unpack();
  this->processor_->dcraw_process();
  return Napi::Value::From(
      info.Env(), this->processor_->dcraw_ppm_tiff_writer(filename.Utf8Value().c_str()));
}

void LibRawWrapper::Recycle(const Napi::CallbackInfo &info)
{
  this->processor_->recycle();
}

Napi::Value LibRawWrapper::ErrorCount(const Napi::CallbackInfo &info)
{
  return Napi::Value::From(
      info.Env(),
      this->processor_->error_count());
}

Napi::Value LibRawWrapper::VersionNumber(const Napi::CallbackInfo &info)
{
  return Napi::Value::From(
      info.Env(),
      this->processor_->versionNumber());
}

Napi::Value LibRawWrapper::Version(const Napi::CallbackInfo &info)
{
  return Napi::Value::From(
      info.Env(),
      this->processor_->version());
}

Napi::Value LibRawWrapper::CameraCount(const Napi::CallbackInfo &info)
{
  return Napi::Value::From(
      info.Env(),
      this->processor_->cameraCount());
}

/*
 * `cameraList` provides a null-terminated list of camera names.
 * This function finds the limit and then allocates an Napi::Array with the
 * found limit, and loads the values to return.
 *
 * This function can almost certainly be refactored for better efficiency and readability.
 */
Napi::Value LibRawWrapper::CameraList(const Napi::CallbackInfo &info)
{
  const char **cameraList = this->processor_->cameraList();
  size_t i = 0;
  while (cameraList[i] != nullptr)
  {
    i++;
  }
  Napi::Array cameraListArray = Napi::Array::New(info.Env(), i);
  for (size_t idx = 0; idx < i; idx++)
  {
    cameraListArray[idx] = cameraList[idx];
  }
  return cameraListArray;
}

Napi::Value LibRawWrapper::StrError(const Napi::CallbackInfo &info)
{
  return Napi::Value::From(
      info.Env(),
      this->processor_->strerror(info[0].As<Napi::Number>().Int32Value()));
}

void LibRawWrapper::RecycleDatastream(const Napi::CallbackInfo &info)
{
  this->processor_->recycle_datastream();
}

LibRawWrapper::~LibRawWrapper()
{
  this->processor_->recycle();
  delete this->processor_;
}
