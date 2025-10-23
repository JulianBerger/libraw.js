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

    if (info.Length() < 1 || !info[0].IsString())
    {
        Napi::TypeError::New(env, "extract_tiff: first argument must be a string output path").ThrowAsJavaScriptException();
        return env.Null();
    }

    // Out Path Argument
    std::string outPath = info[0].As<Napi::String>().Utf8Value();

    if (!this->processor_)
    {
        Napi::Error::New(env, "LibRaw processor not initialised").ThrowAsJavaScriptException();
        return env.Null();
    }

    // Set output/processing params
    auto &p = this->processor_->imgdata.params;

    p.output_tiff      = 1;  // write TIFF
    p.output_bps       = 8;  // 8 for smaller files
    p.output_color     = 1;  // sRGB
    p.use_camera_wb    = 1;  // use camera white balance
    p.use_camera_matrix= 1;  // use camera color matrix

    // Choose either auto-bright or explicit exposure correction (not both aggressively)
    p.no_auto_bright   = 1;          // reproducible brightness; disable auto-bright
    p.auto_bright_thr  = 0.00085f;   // kept for completeness; ignored if no_auto_bright=1

    // Exposure correction (gentle lift, with highlight preservation)
    p.exp_correc       = 1;
    p.exp_shift        = 1.3f;       // tweak 1.0..2.0 as needed
    p.exp_preser       = 0.5f;       // 0..1: preserve highlights

    // Highlight handling: 5 is a common compromise for reconstruction
    p.highlight        = 5;

    // Gamma: sRGB
    // LibRaw expects gamm[0] = 1/gamma_power, gamm[1] = toe_slope
    // For sRGB use 1/2.4 and 12.92; for BT.709 use 1/2.222 and 4.5
    p.gamm[0] = 1.0f / 2.4f;
    p.gamm[1] = 12.92f;

    // Demosaic quality: 2=PPG (fast), 3=AHD (balanced), 4=DCB (high)
    p.user_qual        = 2;

    // Apply camera-provided DefaultCrop (removes masked/optical black borders)
    // raw_inset_crops[0] = DefaultCrop; [1] may hold DefaultUserCrop/aspect ratio crop
    const auto &c = this->processor_->imgdata.sizes.raw_inset_crops[0];
    if (c.cwidth && c.cheight)
    {
        p.cropbox[0] = c.cleft;
        p.cropbox[1] = c.ctop;
        p.cropbox[2] = c.cwidth;
        p.cropbox[3] = c.cheight;
    }

    // Unpack/process/write with robust error handling
    int ret = 0;

    ret = this->processor_->unpack();
    if (ret != LIBRAW_SUCCESS)
    {
        Napi::Error::New(env, std::string("LibRaw unpack() failed: ") + libraw_strerror(ret)).ThrowAsJavaScriptException();
        return env.Null();
    }

    ret = this->processor_->dcraw_process();
    if (ret != LIBRAW_SUCCESS)
    {
        Napi::Error::New(env, std::string("LibRaw dcraw_process() failed: ") + libraw_strerror(ret)).ThrowAsJavaScriptException();
        return env.Null();
    }

    ret = this->processor_->dcraw_ppm_tiff_writer(outPath.c_str());
    if (ret != LIBRAW_SUCCESS)
    {
        Napi::Error::New(env, std::string("LibRaw write failed: ") + libraw_strerror(ret)).ThrowAsJavaScriptException();
        return env.Null();
    }

    // Optionally, expose process warnings to JS
    // if (this->processor_->imgdata.process_warnings) { ... return an object with warnings ... }

    return Napi::Boolean::New(env, true);
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
