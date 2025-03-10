// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/lib/ui/painting/single_frame_codec.h"

#include "flutter/lib/ui/painting/frame_info.h"
#include "flutter/lib/ui/ui_dart_state.h"
#include "third_party/tonic/logging/dart_invoke.h"

namespace flutter {

SingleFrameCodec::SingleFrameCodec(ImageDecoder::ImageDescriptor descriptor)
    : descriptor_(std::move(descriptor)) {}

SingleFrameCodec::~SingleFrameCodec() = default;

int SingleFrameCodec::frameCount() const {
  return 1;
}

int SingleFrameCodec::repetitionCount() const {
  return 0;
}

Dart_Handle SingleFrameCodec::getNextFrame(Dart_Handle callback_handle) {
  if (!Dart_IsClosure(callback_handle)) {
    return tonic::ToDart("Callback must be a function");
  }

  // This has to be valid because this method is called from Dart.
  auto dart_state = UIDartState::Current();

  auto decoder = dart_state->GetImageDecoder();

  if (!decoder) {
    return tonic::ToDart("Image decoder not available.");
  }

  auto raw_callback = new DartPersistentValue(dart_state, callback_handle);

  // We dont want to to put the raw callback in a lambda capture because we have
  // to mutate (i.e destroy) it in the callback. Using MakeCopyable will create
  // a shared pointer for the captures which can be destroyed on any thread. But
  // we have to ensure that the DartPersistentValue is only destroyed on the UI
  // thread.
  decoder->Decode(descriptor_, [raw_callback](auto image) {
    std::unique_ptr<DartPersistentValue> callback(raw_callback);

    auto state = callback->dart_state().lock();

    if (!state) {
      // This is probably because the isolate has been terminated before the
      // image could be decoded.

      return;
    }

    tonic::DartState::Scope scope(state.get());

    auto canvas_image = fml::MakeRefCounted<CanvasImage>();
    canvas_image->set_image(std::move(image));

    auto frame_info = fml::MakeRefCounted<FrameInfo>(std::move(canvas_image),
                                                     0 /* duration */);

    tonic::DartInvoke(callback->value(), {tonic::ToDart(frame_info)});
  });

  return Dart_Null();
}

size_t SingleFrameCodec::GetAllocationSize() {
  const auto& data = descriptor_.data;
  auto data_byte_size = data ? data->size() : 0;
  return data_byte_size + sizeof(this);
}

}  // namespace flutter
