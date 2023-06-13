/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2011-2015  Regents of the University of California.
 *
 * This file is part of ndnSIM. See AUTHORS for complete list of ndnSIM authors and
 * contributors.
 *
 * ndnSIM is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndnSIM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndnSIM, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

// PCS13_ndn-as7018-with-pit-count-stats.cpp

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/log.h"

#include <fstream>
#include <time.h>

NS_LOG_COMPONENT_DEFINE("ndn.Pcs13ndnAs7018Pitstats");

namespace ns3 {

std::ofstream myfile;

std::string myfilename;
std::string node_name;
/**
 * This scenario simulates a AS7018 topology (using PointToPointGrid module)
 *
 * /home/csepg/lalitha_workspace/ndnSIM/ns-3/src/ndnSIM/examples/topologies/7018.r0-conv-annotated.txt
 *    
 * Replaced bb,rx,lf with Node in the annotated topology file   
 *     
 *    
 *
 * All links are 1Mbps with propagation 10ms delay.
 *
 * FIB is populated using NdnGlobalRoutingHelper.
 *
 * Consumer requests data from producer with frequency 100 interests per second
 * (interests contain constantly increasing sequence number).
 *
 * For every received interest, producer replies with a data packet, containing
 * 1024 bytes of virtual payload.
 *
 * To run scenario and see what is happening, use the following command:
 *
 *     NS_LOG=ndn.Consumer:ndn.Producer ./waf --run=ndn-grid
 */

// Get current date/time, format is YYYY-MM-DD.HH:mm:ss
const std::string currentDateTime() {
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
    // for more information about date/time format
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}

void
PeriodicStatsPrinter (Ptr<Node> node, Time next)
{
  
  auto pitSize = node->GetObject<ndn::L3Protocol>()->getForwarder()->getPit().size();
  
  myfile << Simulator::Now ().ToDouble (Time::S) << "\t"
            << node->GetId () << "\t"
            << Names::FindName (node) << "\t"
            << pitSize << "\n";
  
  Simulator::Schedule (next, PeriodicStatsPrinter, node, next);
}

int
main(int argc, char* argv[])
{
  // Setting default parameters for PointToPoint links and channels
  Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("1Mbps"));
  Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("10ms"));
  Config::SetDefault("ns3::DropTailQueue<Packet>::MaxSize", StringValue("10p"));

  // Read optional command-line parameters (e.g., enable visualizer with ./waf --run=<> --visualize
  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Using AS7018 topology
  AnnotatedTopologyReader topologyReader("", 25);
  topologyReader.SetFileName("src/ndnSIM/examples/topologies/7018.r0-conv-annotated.txt");
  topologyReader.Read();

  // Install NDN stack on all nodes
  ndn::StackHelper ndnHelper;
  ndnHelper.InstallAll();

  // Set BestRoute strategy
  ndn::StrategyChoiceHelper::InstallAll("/", "/localhost/nfd/strategy/best-route");

  // Installing global routing interface on all nodes
  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.InstallAll();

  // Getting containers for the consumer/producer
  Ptr<Node> producer = Names::Find<Node>("Node624");
  NodeContainer consumerNodes;
  consumerNodes.Add(Names::Find<Node>("Node329"));
  
  myfilename = "../PCS13_traces/Experiment_2/PCS13_as7018_1000000000ips_PITcnt_" + currentDateTime() + ".txt";

  myfile.open (myfilename,std::ios::out);

  // set up periodic PIT stats printer on all nodes
  myfile << "Time" << "\t"
            << "NodeId" << "\t"
            << "NodeName" << "\t"
            << "NumberOfPitEntries" << "\n";

  for(int i=0;i<625;i++)
  {
    node_name = "Node" + std::to_string(i);
    Simulator::Schedule (Seconds (0), PeriodicStatsPrinter, Names::Find<Node>(node_name), Seconds (0.1));
  }
  
  // Install NDN applications
  std::string prefixPro = "/prefix1";
  std::string prefixCon = "/prefix1";

  // Batches
  ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
  consumerHelper.SetPrefix(prefixCon); //+std::to_string(std::rand() % 625)
  consumerHelper.SetAttribute("Frequency", StringValue("1000000000")); // default 100 interests a second
  consumerHelper.Install(consumerNodes);

  ndn::AppHelper producerHelper("ns3::ndn::Producer");
  producerHelper.SetPrefix(prefixPro);
  producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
  producerHelper.Install(producer);

  // Add /prefix origins to ndn::GlobalRouter
  
  //ndnGlobalRoutingHelper.AddOrigins(prefixPro, producer);
  for(int i=0;i<624;i++)
  {
  /*  if (i == 329)
    {
      //continue; //Skip for Consumer node
    }
    */
    node_name = "Node" + std::to_string(i);
    ndnGlobalRoutingHelper.AddOrigin(prefixPro, node_name);
    //ndnGlobalRoutingHelper.AddOrigin(prefixCon, node_name);
  }

  // Calculate and install FIBs
  ndn::GlobalRoutingHelper::CalculateRoutes();

  Simulator::Stop(Seconds(10.0)); //Default 20 seconds

  // Enable tracing
  
  ndn::L3RateTracer::InstallAll("../PCS13_traces/Experiment_2/PCS13_as7018_1000000000ips_rate-trace.txt", Seconds(0.1));
  L2RateTracer::InstallAll("../PCS13_traces/Experiment_2/PCS13_as7018_1000000000ips_drop-trace.txt", Seconds(0.1));
  ndn::AppDelayTracer::InstallAll("../PCS13_traces/Experiment_2/PCS13_as7018_1000000000ips_app-delays-trace.txt");
  ndn::CsTracer::InstallAll("../PCS13_traces/Experiment_2/PCS13_as7018_1000000000ips_cs-trace.txt", Seconds(0.1));

  Simulator::Run();
  Simulator::Destroy();

  myfile.close();

  
  return 0;
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}
