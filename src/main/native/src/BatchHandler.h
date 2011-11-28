/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef BATCHHANDLER_H_
#define BATCHHANDLER_H_

#include "NativeObject.h"

namespace Hadoop {

/**
 * Native side abstraction of java ByteBuffer
 */
struct ByteBuffer {
  char * buff;
  uint32_t capacity;
  uint32_t position;
  ByteBuffer():buff(NULL),capacity(0),position(0){}
  ~ByteBuffer() {}
  void reset(char * buff, uint32_t capacity) {
    this->buff = buff;
    this->capacity = capacity;
    this->position = 0;
  }
};

/**
 * Native side counterpart of java side NativeBatchProcessor
 * Subclass of BatchHandler should follow the naming rule:
 *  <M|R><StartStage><EndStage>Handler
 *  Map task stages:
 *    Input Reader Mapper Partitioner Collector Combiner Output
 *  Reduce task stages:
 *    Reduce
 */
class BatchHandler: public Hadoop::NativeObject {
protected:
  ByteBuffer _ib;
  ByteBuffer _ob;
  void * _tempJNIEnv;
  void * _tempJProcessor;
  bool _hasJavaException;
public:
  BatchHandler();
  virtual ~BatchHandler();

  virtual NativeObjectType type() {
    return BatchHandlerType;
  }

  /**
   * Called by native jni functions to set local jni reference
   */
  void setTemps(void * jenv, void * processor) {
    _tempJNIEnv = jenv;
    _tempJProcessor = processor;
  }

  /**
   * @return true if java side threw an exception
   */
  bool hasJavaException() {
    return _hasJavaException;
  }

  /**
   * Called by java side to setup native side BatchHandler
   * initialize buffers by default
   */
  void onSetup(char * inputBuffer, uint32_t inputBufferCapacity,
               char * outputBuffer, uint32_t outputBufferCapacity);

  /**
   * Called by java side to notice that input data available to handle
   * @param length input buffer's available data length
   */
  void onInputData(uint32_t length) {
    if (length>_ib.capacity) {
      THROW_EXCEPTION(IOException, "length larger than input buffer capacity");
    }
    _ib.position = length;
    handleInput(_ib.buff, length);
  }

  /**
   * Called by java side to notice that input has finished
   */
  void onFinish() {
    finish();
  }

  /**
   * Called by java side to send command to this handler
   * BatchHandler ignore all command by default
   * @param cmd command data
   * @return command return value
   */
  std::string onCommand(const std::string & cmd) {
    return command(cmd);
  }

protected:
  /**
   * Used by subclass, send command to java side
   * @param data command data
   * @return command return value
   */
  std::string sendCommand(const std::string & cmd);

  /**
   * Used by subclass, call java side flushOutput(int length)
   * @param length output buffer's available data length
   */
  virtual void flushOutput(uint32_t length);

  /**
   * Used by subclass, class java side finishOutput()
   */
  void finishOutput();

  /**
   * Write output buffer and use flushOutput manually,
   * or use this helper method
   */
  inline void put(const char * buff, uint32_t length) {
    while (length>0) {
      if (_ob.position + length > _ob.capacity) {
        flushOutput(_ob.position);
        _ob.position = 0;
      }
      uint32_t remain = _ob.capacity - _ob.position;
      uint32_t cp = length < remain ? length : remain;
      simple_memcpy(_ob.buff+_ob.position, buff, cp);
      buff += cp;
      length -= cp;
      _ob.position += cp;
    }
  }

  inline void putInt(uint32_t v) {
    if (_ob.position + 4 > _ob.capacity) {
      flushOutput(_ob.position);
      _ob.position = 0;
    }
    *(uint32_t*) (_ob.buff + _ob.position) = v;
    _ob.position += 4;
  }

  /**
   * Use flushOutput manually or use this helper method
   */
  inline void flush() {
    if (_ob.position > 0) {
      flushOutput(_ob.position);
      _ob.position = 0;
    }
  }

  /////////////////////////////////////////////////////////////
  // Subclass should implement these if needed
  /////////////////////////////////////////////////////////////

  /**
   * Called by onSetup, do nothing by default
   * Subclass should override this if needed
   */
  virtual void setup() {}

  /**
   * Called by onFinish, flush & close output by default
   * Subclass should override this if needed
   */
  virtual void finish() {
    flush();
    finishOutput();
  };

  /**
   * Called by onInputData, internal input data processor,
   * Subclass should override this if needed
   */
  virtual void handleInput(char * buff, uint32_t length) { }

  /**
   * Called by onCommand, do nothing by default
   * Subclass should override this if needed
   */
  virtual std::string command(const std::string & cmd) {
    return std::string();
  }

};

}

#endif /* BATCHHANDLER_H_ */