/**
 * Copyright (C) 2016-2017 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#ifndef _HW_EM_SHIM_H_
#define _HW_EM_SHIM_H_

// Implementation of Xilinx OpenCL low level platform APIs for hw_em
// This is layered on top of hw_ems Linux kernel driver.
#include <sys/param.h>
#include "rpc_messages.pb.h"
#include "xbar_sys_parameters.h"
#ifndef _WINDOWS
// TODO: Windows build support
// unix_socket.h is linux only
// We need to provide a Windows equivalent functionality
#include "unix_socket.h"
#endif

#include "xcl_api_macros.h"
#include "xcl_macros.h"
#include <cstring>
#include <algorithm>
#include <google/protobuf/message_lite.h>
#include <sstream>
#include "em_defines.h"
#include "xclhal2.h"
#include "xclbin.h"
#include "xclperf.h"
#include "HPIXclbinXmlReaderWriterLMX.h"
#include "lmx6.0/lmxparse.h"

#include <list>
#include <map>
#include <tuple>
#include <sys/types.h>
#include<sys/stat.h>
#include <mutex>
#include <thread>
#include <iostream>
#include <fstream>
#include <signal.h>
#include <vector>
#include "memorymanager.h"
#include "mem_model.h"
#include "mbscheduler.h"
#ifdef _MSC_VER
#include <boost/config/compiler/visualc.hpp>
#endif
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include <cassert>
#include <exception>
#ifdef _WINDOWS
#define strtoll _strtoi64
#endif

#ifdef WINDOWS
#include <direct.h>
#define GetCurrentDir _getcwd
#else
#include <unistd.h>
#define GetCurrentDir getcwd
#endif  
const uint64_t mNullBO = 0xffffffff;

namespace xclhwemhal2 {
using addr_type = uint64_t;
#define PRINTENDFUNC if (mLogStream.is_open()) mLogStream << __func__ << " ended " << std::endl;

  class Event {
    public:
      uint8_t awlen;
      uint8_t arlen;
      uint8_t eventflags;
      uint32_t timestamp;
      uint64_t host_timestamp;
      uint16_t readBytes;
      uint16_t writeBytes;
      Event();
  };
 struct membank
  {
    addr_type base_addr; // base address of bank
    std::string tag;     // bank tag in lowercase
    uint64_t size;       // size of this bank in bytes
    int32_t index;       // bank id
  };


//  struct MemTopology {
//    int32_t             bank_count;
//    struct mem_data*    m_data;
//    u32                 m_data_length; //length of the mem_data section.
//    uint64_t            bank_size; //in KB. Currently only fixed sizes are supported.
//    uint64_t	    size;
//    struct mem_topology *topology;
//  };

typedef struct {
    std::string name;
    unsigned int size;
  } KernelArg;

  class HwEmShim {

    public:

      // HAL2 RELATED member functions start
      unsigned int xclAllocBO(size_t size, xclBOKind domain, unsigned flags);
      int xoclCreateBo(xocl_create_bo *info);
      void* xclMapBO(unsigned int boHandle, bool write);
      int xclSyncBO(unsigned int boHandle, xclBOSyncDirection dir, size_t size, size_t offset); 
      unsigned int xclAllocUserPtrBO(void *userptr, size_t size, unsigned flags);
      int xclGetBOProperties(unsigned int boHandle, xclBOProperties *properties);
      size_t xclWriteBO(unsigned int boHandle, const void *src, size_t size, size_t seek);
      size_t xclReadBO(unsigned int boHandle, void *dst, size_t size, size_t skip);
      void xclFreeBO(unsigned int boHandle);
      int xclExportBO(unsigned int boHandle); 
      unsigned int xclImportBO(int boGlobalHandle);

      //MB scheduler related API's
      int xclExecBuf( unsigned int cmdBO);
      int xclRegisterEventNotify( unsigned int userInterrupt, int fd);
      int xclExecWait( int timeoutMilliSec);
      struct exec_core* getExecCore() { return mCore; }
      MBScheduler* getScheduler() { return mMBSch; }

      drm_xocl_bo* xclGetBoByHandle(unsigned int boHandle);
      inline unsigned short xocl_ddr_channel_count();
      inline unsigned long long xocl_ddr_channel_size();
      // HAL2 RELATED member functions end 
      
      // Bitstreams
      int xclLoadXclBin(const xclBin *buffer);
      //int xclLoadBitstream(const char *fileName);
      int xclLoadBitstreamWorker(char* zipFile, size_t zipFileSize, char* xmlfile, size_t xmlFileSize,
                                 char* debugFile, size_t debugFileSize, char* memTopology, size_t memTopologySize);
      bool isUltraScale() const;
      int xclUpgradeFirmware(const char *fileName);
      int xclBootFPGA();
      int resetProgram(bool saveWdb=true);
      int xclGetDeviceInfo2(xclDeviceInfo2 *info);

      //following functions are created for emulation diagnostics
      bool validateXclBin(const std::string& xmlfileName, Xclbin::Platform& platform, Xclbin::Core& core,std::string &xclBinName);
      size_t getMinSaxiControlReMap(Xclbin::Core& core);
      void populateKernelArgInfo(const Xclbin::Kernel& kernel, std::map<uint64_t,KernelArg>& kernelArgInfo);
      bool getSaxiControlRemap(const Xclbin::Instance &instance, size_t &saxiControlMap);

      // Raw read/write
      size_t xclWrite(xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size);
      size_t xclRead(xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size);
      size_t xclReadModifyWrite(uint64_t offset, const void *hostBuf, size_t size);
      size_t xclReadSkipCopy(uint64_t offset, void *hostBuf, size_t size);

      // Buffer management
      uint64_t xclAllocDeviceBuffer(size_t size);
      uint64_t xclAllocDeviceBuffer2(size_t& size, xclMemoryDomains domain, unsigned flags);

      void xclOpen(const char* logfileName);
      void xclFreeDeviceBuffer(uint64_t buf);
      size_t xclCopyBufferHost2Device(uint64_t dest, const void *src, size_t size, size_t seek, uint32_t topology);
      size_t xclCopyBufferDevice2Host(void *dest, uint64_t src, size_t size, size_t skip, uint32_t topology);
      void xclClose();
      unsigned int xclProbe();

      //Performance Monitor APIs
      double xclGetDeviceClockFreqMHz();
      double xclGetReadMaxBandwidthMBps();
      double xclGetWriteMaxBandwidthMBps();
      size_t xclPerfMonClockTraining();
      size_t xclPerfMonStartCounters();
      size_t xclPerfMonStopCounters();
      uint32_t getPerfMonNumberSlots(xclPerfMonType type);
      size_t xclPerfMonReadCounters( xclPerfMonType type, xclCounterResults& counterResults);
      size_t xclPerfMonStartTrace(uint32_t startTrigger);
      size_t xclPerfMonStopTrace();
      uint32_t xclPerfMonGetTraceCount(xclPerfMonType type);
      size_t xclPerfMonReadTrace(xclPerfMonType type, xclTraceResultsVector& traceVector);
      size_t xclGetDeviceTimestamp();
      void xclReadBusStatus(xclPerfMonType type);
      void xclGetDebugMessages(bool force = false);
      void logMessage(std::string& msg,int verbosity = 0);

      // debug/profiling helpers
      void readDebugIpLayout(const std::string debugFileName);
      uint32_t getIPCountAddrNames(const std::string debugFileName, int type, uint64_t *baseAddress,
                                   std::string * portNames, uint8_t *properties, size_t size);
      void getPerfMonSlotName(xclPerfMonType type, uint32_t slotnum, char* slotName, uint32_t length);

      //Utility Function
      void set_simulator_started(bool val){ simulator_started = val;}
      void fillDeviceInfo(xclDeviceInfo2* dest, xclDeviceInfo2* src);
      void saveWaveDataBase();

      // Sanity checks
      static HwEmShim *handleCheck(void *handle);
      uint32_t getAddressSpace (uint32_t topology);

      //constructor
      HwEmShim( unsigned int deviceIndex, xclDeviceInfo2 &info, std::list<xclemulation::DDRBank>& DDRBankList, bool bUnified, bool bXPR);

      //destructor
      ~HwEmShim();

      static const int SPIR_ADDRSPACE_PRIVATE;  //0
      static const int SPIR_ADDRSPACE_GLOBAL;   //1
      static const int SPIR_ADDRSPACE_CONSTANT; //2
      static const int SPIR_ADDRSPACE_LOCAL;    //3
      static const int SPIR_ADDRSPACE_PIPES;    //4

      static const unsigned CONTROL_AP_START;
      static const unsigned CONTROL_AP_DONE;
      static const unsigned CONTROL_AP_IDLE;

      bool isUnified()               { return bUnified; }
      void setUnified(bool _unified) { bUnified = _unified; }

      bool isXPR()           { return bXPR; }
      void setXPR(bool _xpr) { bXPR = _xpr; }
      std::string deviceDirectory;

    private:
      //hw_em_profile* _profile_inst;
      bool simulator_started;
      uint64_t mRAMSize;
      size_t mCoalesceThreshold;
      void launchTempProcess() {};

      void initMemoryManager(std::list<xclemulation::DDRBank>& DDRBankList);
      std::vector<xclemulation::MemoryManager *> mDDRMemoryManager;
      std::list<xclemulation::DDRBank> mDdrBanks;
      std::map<uint64_t,std::map<uint64_t, KernelArg>> mKernelOffsetArgsInfoMap;
      std::map<uint64_t,uint64_t> mAddrMap;
      std::map<std::string,std::string> mBinaryDirectories;
      std::map<uint64_t , std::ofstream*> mOffsetInstanceStreamMap;

      //mutex to control parellel RPC calls
      std::mutex mtx;
      std::mutex mApiMtx;
      std::vector<Event> list_of_events[XSPM_MAX_NUMBER_SLOTS];
      unsigned int tracecount_calls;
      // In case support for different version DSAs is required
      int mDSAMajorVersion;
      int mDSAMinorVersion;
      static std::map<std::string, std::string> mEnvironmentNameValueMap;

      void* ci_buf;
      call_packet_info ci_msg;

      response_packet_info ri_msg;
      void* ri_buf;
      size_t alloc_void(size_t new_size);

      void* buf;
      size_t buf_size;
      std::ofstream mLogStream;
      std::ofstream mGlobalInMemStream;
      std::ofstream mGlobalOutMemStream;
      static std::ofstream mDebugLogStream;
      static bool mFirstBinary;
      unsigned int binaryCounter;
      unix_socket* sock;
      std::string deviceName;
      xclDeviceInfo2 mDeviceInfo;
      unsigned int mDeviceIndex;
      clock_t last_clk_time;
      bool mCloseAll;
      mem_model* mMemModel;
      bool bUnified;
      bool bXPR;
      //MemTopology topology;
      // HAL2 RELATED member variables start
      std::map<int, drm_xocl_bo*> mXoclObjMap;
      static unsigned int mBufferCount;
      // HAL2 RELATED member variables end 
      exec_core* mCore;
      MBScheduler* mMBSch;
      
      // Information extracted from platform linker (for profile/debug)
      bool mIsDebugIpLayoutRead = false;
      bool mIsDeviceProfiling = false;
      uint32_t mMemoryProfilingNumberSlots;
      uint32_t mAccelProfilingNumberSlots;
      uint32_t mStallProfilingNumberSlots;
      uint64_t mPerfMonFifoCtrlBaseAddress;
      uint64_t mPerfMonFifoReadBaseAddress;
      uint64_t mPerfMonBaseAddress[XSPM_MAX_NUMBER_SLOTS];
      uint64_t mAccelMonBaseAddress[XSAM_MAX_NUMBER_SLOTS];
      std::string mPerfMonSlotName[XSPM_MAX_NUMBER_SLOTS];
      std::string mAccelMonSlotName[XSAM_MAX_NUMBER_SLOTS];
      uint8_t mPerfmonProperties[XSPM_MAX_NUMBER_SLOTS];
      uint8_t mAccelmonProperties[XSAM_MAX_NUMBER_SLOTS];
      std::vector<membank> mMembanks;
  };

  extern std::map<unsigned int, HwEmShim*> devices;
 }
#endif

