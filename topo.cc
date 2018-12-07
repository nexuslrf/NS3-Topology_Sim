/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
// SJTU CS339 COMPUTER NETWORK PROJECT 2
// Ruofan Liang shootingstarlrf@hotmail.com
// Using topology-read-module to read topology from ./topo.txt

#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include "ns3/topology-read-module.h"
#include <list>
#include <ctime>
#include <sstream>
#include <time.h>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-nix-vector-helper.h"
#include "ns3/delay-jitter-estimation.h"
#include "ns3/traffic-control-module.h"
using namespace ns3;

NetDeviceContainer ndc[100];
NodeContainer nc[100];
DelayJitterEstimation delayJitter;

static void CalculateLen()
{
  int max = -1;
  int tmp;
  for (int i = 0; i < 100; i++)
  {
    tmp = 0;
    int size = ndc[i].GetN();
    for (int j = 0; j < size; j++)
    {

      tmp = tmp + ndc[i].Get(j)->GetObject<PointToPointNetDevice>()->GetQueue()->GetNBytes();
    }
    if (tmp > max)
    {
      max = tmp;
    }
  }
  std::cout <<"Max_Packet_Queue_Len: "<< max <<" Time: "<<Simulator::Now().GetSeconds()<<std::endl;
}

static void
CalculateDelay(Ptr<const Packet> p, const Address &address)
{
  delayJitter.RecordRx(p);
  Time t = delayJitter.GetLastDelay();
  std::cout <<"Delay: "<< Simulator::Now().GetSeconds() << "\t" << t.GetNanoSeconds()<<" ns" << std::endl;
}

static void
TagTx(Ptr<const Packet> p)
{
  delayJitter.PrepareTx(p);
}

NS_LOG_COMPONENT_DEFINE("CNPrj_LRF");
std::vector<int> random_pair()
{

  std::vector<int> l;
  int end = 49;
  int begin = 0;
  int sum = (rand() % (25) + 1) * 2;
  while (l.size() < (unsigned)(sum))
  {
    l.push_back(rand() % (end - begin + 1) + begin);
  }
  return l;
}
int main(int argc, char *argv[])
{
  srand((unsigned)time(NULL));
  // Users may find it convenient to turn on explicit debugging
  // for selected modules; the below lines suggest how to do this
  // #if 0
  LogComponentEnable("CNPrj_LRF", LOG_LEVEL_ALL);
  //LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  //LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // #endif

  std::string format("Inet");
  std::string input("./scratch/topo.txt");

  // Set up command line parameters used to control the experiment.
  CommandLine cmd;
  cmd.AddValue("format", "Format to use for data input [Orbis|Inet|Rocketfuel].",
               format);
  cmd.AddValue("input", "Name of the input file.",
               input);
  cmd.Parse(argc, argv);

  // ------------------------------------------------------------
  // -- Read topology data.
  // --------------------------------------------

  // Pick a topology reader based in the requested format.
  TopologyReaderHelper topoHelp;
  topoHelp.SetFileName(input);
  topoHelp.SetFileType(format);
  Ptr<TopologyReader> inFile = topoHelp.GetTopologyReader();

  NodeContainer nodes;
  if (inFile != 0)
  {
    nodes = inFile->Read();
  }

  if (inFile->LinksSize() == 0)
  {
    NS_LOG_ERROR("Problems reading the topology file. Failing.");
    return -1;
  }
  NS_LOG_INFO("read the file");
  NodeContainer routers;
  NodeContainer terminals;
  std::cout << nodes.GetN() << ' ' << std::endl;
  for (int i = 0; i < 50; i++)
  {
    terminals.Add(nodes.Get(i));
  }
  for (int i = 50; i < 80; i++)
  {
    routers.Add(nodes.Get(i));
  }

  // ------------------------------------------------------------
  // -- Create nodes and network stacks
  // --------------------------------------------
  NS_LOG_INFO("creating internet stack");

   InternetStackHelper stack_router;
   Ipv4ListRoutingHelper list;


   RipHelper ripRouting;
   // Rule of thumb:
   // Interfaces are added sequentially, starting from 0
   // However, interface 0 is always the loopback...
   std::vector<int> nodes_Interface(100, 0);
   int i = 0;
   for (TopologyReader::ConstLinksIterator iter = inFile->LinksBegin(); i < 50; iter++, i++)
   {
     ripRouting.ExcludeInterface(iter->GetToNode(), ++nodes_Interface[iter->GetToNode()->GetId()]);
   }
   
   Ipv4NixVectorHelper nixRouting;        
   OlsrHelper olsr;
   Ipv4StaticRoutingHelper staticRouting;
   
   list.Add(ripRouting, 5);
   list.Add (staticRouting, 0);
   list.Add (olsr, 10);

   stack_router.SetIpv6StackInstall(false);
   stack_router.SetRoutingHelper(list);
   stack_router.Install(nodes);

  NS_LOG_INFO("creating ip4 addresses");
  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");

  int totlinks = inFile->LinksSize();

  NS_LOG_INFO("creating node containers");
  TopologyReader::ConstLinksIterator iter;
  i = 0;
  for (TopologyReader::ConstLinksIterator iter = inFile->LinksBegin(); iter != inFile->LinksEnd(); iter++, i++)
  {
    nc[i] = NodeContainer(iter->GetFromNode(), iter->GetToNode());
  }

  NS_LOG_INFO("creating net device containers");
  PointToPointHelper p2p;
  p2p.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("2MB"));
  // error model
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
  em->SetAttribute("ErrorRate", DoubleValue(0.0001));
  for (int i = 0; i < totlinks; i++)
  {
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetDeviceAttribute("InterframeGap", StringValue("0.333ms"));
    ndc[i] = p2p.Install(nc[i]);
    ndc[i].Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
    ndc[i].Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));
  }

  // it crates little subnets, one for each couple of nodes.
  NS_LOG_INFO("creating ipv4 interfaces");
  Ipv4InterfaceContainer *ipic = new Ipv4InterfaceContainer[totlinks];
  for (int i = 0; i < totlinks; i++)
  {
    ipic[i] = address.Assign(ndc[i]);
    address.NewNetwork();
  }

  
  NS_LOG_INFO("creating client");
  ApplicationContainer clientContainer;
  int port = 1;
  UdpEchoClientHelper client(ipic[0].GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(100));
  client.SetAttribute("Interval", TimeValue(Seconds(0.00375)));
  client.SetAttribute("PacketSize", UintegerValue(210));

  double current_time;
  std::vector<int> host_pair;
  int pair_num;
  int current_pair;
  double total_time = 120.0;
  for (current_time = 0.0; current_time < total_time; current_time += 0.1)
  {
    host_pair = random_pair();
    pair_num = host_pair.size() / 2;
    for (current_pair = 0; current_pair < pair_num; ++current_pair)
    {
      if(2 * current_pair == 2 * current_pair + 1)
          continue;
      client.SetAttribute("RemoteAddress", AddressValue(ipic[host_pair[2 * current_pair + 1]].GetAddress(0)));
      clientContainer = client.Install(terminals.Get(host_pair[2 * current_pair]));
      clientContainer.Start(Seconds(current_time));
      clientContainer.Stop(Seconds(current_time + 0.1));
      clientContainer.Get(0)->TraceConnectWithoutContext("Tx", MakeCallback(&TagTx));
    }
  }

  PacketSinkHelper sink("ns3::UdpSocketFactory",
                        InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = sink.Install(terminals);
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(120.0));

  for (i = 0; i < 50; i++)
  {
    sinkApps.Get(i)->TraceConnectWithoutContext("Rx", MakeCallback(&CalculateDelay));
  }

  //p2p.EnablePcapAll ("routing-simulator");*/

  Simulator::Stop(Seconds(120));
  for (int i = 0; i <= 1200; i++)
  {
    Simulator::Schedule(MilliSeconds(100 + 100 * i), CalculateLen);
  }

  NS_LOG_INFO("Run Simulation.");
  Simulator::Run();
  Simulator::Destroy();
  NS_LOG_INFO("Done.");

  return 0;
}
