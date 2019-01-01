// Adapted from ZMQROOTmerger.cxx in HLT/BASE/utils in AliRoot
//
// Author: Raymond Ehlers <raymond.ehlers@cern.ch>, Yale University

#include "zmqReceiver.h"

#include <zmq.h>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <ctime>
// For mkdir
#include <sys/stat.h>

// For timing
#include <chrono>
#include <thread>

#include <TObject.h>
#include <TFile.h>
// If desired for more complicated selections
//#include <TPRegexp.h>

#include <AliZMQhelpers.h>
#include <AliOptionParser.h>

using namespace AliZMQhelpers;

volatile sig_atomic_t zmqReceiver::fgSignalCaught = 0;

const std::string zmqReceiver::fgUsage =
  "zmqReceive\n"
  "    Receive ROOT objects from the HLT via ZMQ.\n\n"
  "Options:\n"
  "    --in <address>: address for incoming ZMQ data. Format should be \"MODE>tcp://address:port\".\n"
  "              For example: \"REQ>tcp://localhost:1234\"\n"
  "    --verbose <level>: Control verbosity level. Disable with 0. Default: 1.\n"
  "    --resetMerger: Reset the merger after each request. Use with care! Default: false\n"
  "    --requestStreamers: Request ROOT streamers from the mergers. Default: true\n"
  "    --select <string>: Selection string to request data from the merger.\n"
  "              Defaults to \"\" (ie No special selection).\n"
  "    --dataPath <string>: Path to the data directory.\n"
  "    --sleep <seconds>: Time to sleep between each request in seconds. Default: 60.\n"
  "    --timeout <seconds>: Time to wait for a response to a request in seconds. Default: 10.\n"
  ;

/**
 * Handle caught SIGINT signal.
 */
void zmqReceiver::caughtSignal(int i)
{
  if (i == SIGINT) {
    printf("Caught SIGINT. Terminating!\n");
  }
  fgSignalCaught = i;
}

/**
 * Default constructor.
 */
zmqReceiver::zmqReceiver():
  fVerbose(0),
  fRunNumber(123456789),
  fResetMerger(false),
  fSubsystem("EMC"),
  fRequestStreamers(true),
  fHLTMode("B"),
  fSelection(""),
  fDataPath("."),
  fPollInterval(60000),
  fPollTimeout(10000),
  fZMQconfigIn("SUB>tcp://localhost:60201"),
  fData(),
  fZMQcontext(NULL),
  fZMQin(NULL)
{
  // Create the ZMQ context
  fZMQcontext = alizmq_context();
}

/**
 * Main function. It will loop indefinitely making requests to the merger.
 */
int zmqReceiver::Run()
{
  // Show the current configuration
  std::cout << PrintConfiguration() << "\n";

  // Register signal hanlder
  // See: https://stackoverflow.com/a/1641223
  //      http://zguide.zeromq.org/cpp:interrupt
  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = this->caughtSignal;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

  // Main loop
  while (1) {
    // Write heartbeat to a file to show that the receiver is alive.
    WriteHeartbeat();

    errno = 0;

    // Request the data
    SendRequest();

    // Define ZMQ sockets
    zmq_pollitem_t sockets[] = {
      {fZMQin, 0, ZMQ_POLLIN, 0},
    };

    // Wait for the data by polling sockets.
    // The second argument denotes the number of sockets that are being
    // polled, while the third argument is the timeout.
    int rc = zmq_poll(sockets, 1, fPollTimeout);
    if (rc == -1 && errno == ETERM) {
      // This can only happen it the context was terminated, one of the
      // sockets are not valid, or operation was interrupted
      std::cout << "ZMQ context was terminated! Bailing out! rc = " << rc << ", " << zmq_strerror(errno) << "\n";
      return -1;
    }

    // If we caught ctrl-c, then break so we can close the sockets
    // NOTE: This must be done before handling if the server is dead because
    //       otherwise it will attempt to re-init the socket and continue
    //       instead of terminating!
    if (fgSignalCaught) {
      break;
    }

    // Handle if the request timed out (perhaps due to a dead server).
    if (!(sockets[0].revents & ZMQ_POLLIN)) {
      // Server died
      std::cout << "Connection timed out. Server " << fZMQconfigIn << " died?\n";
      int fZMQsocketModeIn = alizmq_socket_init(fZMQin, fZMQcontext, fZMQconfigIn.c_str());
      if (fVerbose) {
        std::cout << fZMQsocketModeIn << "\n";
      }
      if (fZMQsocketModeIn < 0) {
        std::cout << "Cannot reinit ZMQ socket " << fZMQconfigIn << ", " << zmq_strerror(errno) << ", exiting...\n";
        return -1;
      }

      // If we managed to reinitialize the socket, then we start over again.
      continue;
    }
    // Data present in socket 0 (ie fZMQin)
    else if (sockets[0].revents & ZMQ_POLLIN) {
      ReceiveData();
    }

    // Sleep so that we are not constantly requesting data
    std::this_thread::sleep_for(std::chrono::milliseconds(fPollInterval));
  }

  return 0;
}

/**
 * Write a unix timestamp to file to act as a heartbeat for the receiver running properly.
 */
void zmqReceiver::WriteHeartbeat()
{
  std::string filename = fDataPath + "/heartbeat." + fSubsystem + "Receiver";
  std::ofstream heartbeat(filename);
  heartbeat << std::time(nullptr);
  heartbeat.close();
}

/**
 * Handles receiving the data from the merger. The message will be packed into 
 * an aliZMQmsg.
 */
void zmqReceiver::ReceiveData()
{
  // Clear previous data
  ClearData();

  // Receive message
  aliZMQmsg message;
  alizmq_msg_recv(&message, fZMQin, 0);

  // Processing message data
  for (aliZMQmsg::iterator i = message.begin(); i != message.end(); ++i) {
    // Check for information about the data
    if (alizmq_msg_iter_check_id(i, AliZMQhelpers::kDataTypeInfo) == 0) {
      // Retrieve info about the data.
      std::string info;
      alizmq_msg_iter_data(i, info);
      if (fVerbose) {
        std::cout << "processing INFO " << info << "\n";
      }

      // Parse the info string.
      stringMap fInfoMap = ParseParamString(info);

      // Retrieve run number and HLT mode.
      fRunNumber = atoi(fInfoMap["run"].c_str());
      fHLTMode = fInfoMap["HLT_MODE"];

      if (fVerbose) {
        std::cout << "Received:\n\tRun Number: " << fRunNumber << "\n\tHLT Mode: " << fHLTMode << "\n";
      }

      // Now move onto processing the actual data.
      continue;
    }

    // Check for and retrieve streamer information and make it available to ROOT.
    if (alizmq_msg_iter_check_id(i, AliZMQhelpers::kDataTypeStreamerInfos) == 0) {
      alizmq_msg_iter_init_streamer_infos(i);
    }

    // Store the data to be written out.
    TObject* object;
    int returnValue = alizmq_msg_iter_data(i, object);
    if (returnValue != 0) {
      // Something went wrong.
      if (returnValue > 0) {
        // Something went wrong in the deserialization, so we need to
        // re-request the streamers for this object. Skip it for now to
        // avoid corrupting the file.
        fRequestStreamers = true;
        continue;
      } else {
        // Whatever this payload is, it doesn't appear to be a ROOT
        // object. Log the it happened and skip the object so we don't
        // crash due to trying to write a nullptr.
        std::cout << "Object at position " << std::distance(message.begin(), i) << " does not appear to be a ROOT object and will be skipped!\n";
        continue;
      }
    }
    fData.push_back(object);
  }

  // Close message
  alizmq_msg_close(&message);

  // The HLT sends run number 0 after it is has reset receivers at the end of
  // a run. We shouldn't bother writing out the file in that case. We also
  // shouldn't try to write an empty file if we haven't received any data.
  if (fRunNumber != 0 && fData.size() != 0) {
    // Write Data
    WriteToFile();
  } else {
    if (fRunNumber != 0) {
      std::cout << "fRunNumber == 0. Not printing, since this is not a real run!\n";
    } else {
      std::cout << "No new data to write. Waiting for next request.\n";
    }
  }
}

/**
 * Write the received data to a file. The filename includes the subsystem, HLT mode, run number, and timestamp.
 * The time stamp is of the format year_month_day_hour_minute_second . 
 *
 * The particular filename format is:
 * SUBSYSTEMhistos_runNumber_hltMode_time.root
 * For example, EMChistos_123456_B_2015_3_14_2_3_5.root
 */
void zmqReceiver::WriteToFile()
{
  // Get current time (in the local timezone).
  time_t now = time(NULL);
  struct tm* timestamp = localtime(&now);

  // Format is SUBSYSTEMhistos_runNumber_hltMode_time.root
  // For example, EMChistos_123456_B_2015_3_14_2_3_5.root
  TString filename = TString::Format("%shistos_%d_%s_%d_%d_%d_%d_%d_%d.root", fSubsystem.c_str(), fRunNumber, fHLTMode.c_str(), timestamp->tm_year+1900, timestamp->tm_mon+1, timestamp->tm_mday, timestamp->tm_hour, timestamp->tm_min, timestamp->tm_sec);
  filename = fDataPath + "/" + filename;

  // Create file
  TFile* fOut = new TFile(filename.Data(), "RECREATE");

  if (fVerbose) {
    std::cout << "Writing " << fData.size() << " objects to " << filename.Data() << "\n";
  }

  // Iterate over all objects and write them to the file
  for (std::vector<TObject*>::iterator it = fData.begin(); it != fData.end(); ++it)
  {
    if (fVerbose) {
      std::cout << "Writing object " << (*it)->GetName() << " to " << filename << "\n";
    }
    (*it)->Write((*it)->GetName());
  }

  // Close file
  fOut->Close();
  delete fOut;
}

/**
 * Send a request to the merger with specified options. The options should be specified on the commend line
 */
void zmqReceiver::SendRequest()
{
  std::string request = "";
  if (fSelection != "") {
    request += " -select=";
    request += fSelection.c_str();
  }
  if (fResetMerger == true) {
    request += " -ResetOnRequest";
  }
  if (fRequestStreamers == true) {
    // We only want to make this request once. Otherwise, we can cause problems within the mergers.
    // This should only be triggered on the first request (if the command line argument is specified),
    // or if the deserialization went wrong (in which can, we try to request the streamers again).
    fRequestStreamers = false;
    request += " -SchemaOnRequest";
  }

  if (fVerbose) {
    std::cout << "\nsending request CONFIG with request \"" << request << "\"\n";
  }
  alizmq_msg_send("CONFIG", request, fZMQin, ZMQ_SNDMORE);
  alizmq_msg_send("", "", fZMQin, 0);
}

/**
 * Clear the data stored from the previous message.
 */
void zmqReceiver::ClearData()
{
  for (std::vector<TObject*>::iterator it = fData.begin(); it != fData.end(); ++it)
  {
    delete *it;
  }
  fData.clear();
}

int zmqReceiver::Init()
{
  // Setup data path
  // Remove trailing slash if it exists for consistency.
  // We use TString for convenience.
  TString dataPath = fDataPath;
  fDataPath = dataPath.Strip(TString::kTrailing, '/').Data();
  // Ensure that fDataPath exists
  // It creates it with read, write, and execute permissions.
  // If the directory already exists, it will fail silently.
  mkdir(fDataPath.c_str(), S_IRWXU);

  int retVal = 0;
  retVal = InitZMQ();

  return retVal;
}

/**
 * Initialize ZMQ socket(s)
 */
int zmqReceiver::InitZMQ()
{
  // Init or reinit ZMQ socket
  int rc = 0;
  rc += alizmq_socket_init(fZMQin, fZMQcontext, fZMQconfigIn.c_str());

  return rc;
}

/**
 * Close all sockets and destroy the ZMQ content to cleanup on close.
 */
void zmqReceiver::Cleanup()
{
  // Destory zmq sockets
  alizmq_socket_close(fZMQin);

  // Terminate the context
  zmq_ctx_term(fZMQcontext);
}

/**
 * Print the configuration of the task.
 */
std::string zmqReceiver::PrintConfiguration()
{
  std::stringstream status;
  status << std::boolalpha;
  status << "Running receiver with configuration:\n";
  status << "\tSubsystem: \"" << fSubsystem << "\"\n";
  status << "\tVerbosity: " << fVerbose << "\n";
  status << "\tSelection: \"" << fSelection << "\"\n";
  status << "\tData directory: \"" << fDataPath << "\"\n";
  status << "\tRequest ROOT streamers: " << fRequestStreamers << "\n";
  status << "\tResetMerger: " << fResetMerger << "\n";
  status << "\tSleep time between requests: " << fPollInterval / 1e3 << " s\n";
  status << "\tRequest timeout: " << fPollTimeout / 1e3 << " s\n";
  status << "\tZMQ In Configuration: " << fZMQconfigIn << "\n";

  return status.str();
}

////////////////////////////////////////////////////////////////////////////////
// Handle command line options
////////////////////////////////////////////////////////////////////////////////

/**
 * Convert options from the terminal to settings in the task.
 *
 * @param[in] option String containing the option name.
 * @param[in] value String containing value corresponding to that option.
 */
int zmqReceiver::ProcessOption(TString option, TString value)
{
  //process option
  //to be implemented by the user

  if (option.EqualTo("ZMQconfigIN") || option.EqualTo("in")) {
    fZMQconfigIn = value;
  } else if (option.EqualTo("verbose")) {
    fVerbose = atoi(value);
  } else if (option.EqualTo("select")) {
    fSelection = value;
  } else if (option.EqualTo("dataPath")) {
    fDataPath = value;
  } else if (option.EqualTo("requestStreamers")) {
    fRequestStreamers = true;
  } else if (option.EqualTo("resetMerger")) {
    fResetMerger = true;
  } else if (option.EqualTo("subsystem")) {
    fSubsystem = value;
  } else if (option.EqualTo("PollInterval") || option.EqualTo("sleep")) {
    fPollInterval = round(value.Atof() * 1e3);
  } else if (option.EqualTo("PollTimeout") || option.EqualTo("timeout")) {
    fPollTimeout = round(value.Atof() * 1e3);
  } else {
    return -1;
  }

  return 1; 
}

/**
 * Convenience function to process options directly from the terminal.
 */
int zmqReceiver::ProcessOptions(int argc, char * argv[])
{
  // Process args using the HLT infrastructure for simplicity
  return ProcessOptionString(AliOptionParser::GetFullArgString(argc,argv));
}

/**
 * Handle the processing of command line options.
 */
int zmqReceiver::ProcessOptionString(TString arguments)
{
  // Process passed options
  aliStringVec* options = AliOptionParser::TokenizeOptionString(arguments);
  int nOptions = 0;
  for (aliStringVec::iterator i = options->begin(); i != options->end(); ++i)
  {
    if (ProcessOption(i->first, i->second) < 0) {
      // If an option is not found, then print the help
      nOptions = -1;
      break;
    }

    // Keep track of number succssfully parsed
    nOptions++;
  }

  // tidy up
  delete options;

  return nOptions;
}
