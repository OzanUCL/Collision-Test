/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015 Sébastien Deronne
 *
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
 * Author: Sébastien Deronne <sebastien.deronne@gmail.com>
 */

#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/log.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/bulk-send-helper.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"

// This example considers two hidden stations in an 802.11n network which supports MPDU aggregation.
// The user can specify whether RTS/CTS is used and can set the number of aggregated MPDUs.
//
// Example: ./waf --run "simple-ht-hidden-stations --enableRts=1 --nMpdus=8"
//
// Network topology:
//
//   Wifi 192.168.1.0
//
//  AP1  St1  St2  AP2
//   *    *    *    *
//   |    |    |    |
//   n0   n1   n2   n3
//
// Packets in this simulation aren't marked with a QosTag so they are considered
// belonging to BestEffort Access Class (AC_BE).

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimplesHtHiddenStations");

int main (int argc, char *argv[])
{
  uint32_t payloadSize = 1472; //bytes
  double simulationTime = 100; //seconds
//  uint32_t nMpdus = 1;
  uint32_t nMpdus = 4;
  uint32_t maxAmpduSize = 0;
  bool enableRts = 0;
  double minExpectedThroughput = 0;
  double maxExpectedThroughput = 0;
 // uint32_t maxBytes = 0;

  CommandLine cmd;
  cmd.AddValue ("nMpdus", "Number of aggregated MPDUs", nMpdus);
  cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue ("enableRts", "Enable RTS/CTS", enableRts);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("minExpectedThroughput", "if set, simulation fails if the lowest throughput is below this value", minExpectedThroughput);
  cmd.AddValue ("maxExpectedThroughput", "if set, simulation fails if the highest throughput is above this value", maxExpectedThroughput);
  cmd.Parse (argc, argv);

  if (!enableRts)
    {
      Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("999999"));
    }
  else
    {
      Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("0"));
    }

  //Set the maximum size for A-MPDU with regards to the payload size
  maxAmpduSize = nMpdus * (payloadSize + 200);

  // Set the maximum wireless range to 5 meters in order to reproduce a hidden nodes scenario, i.e. the distance between hidden stations is larger than 5 meters
  Config::SetDefault ("ns3::RangePropagationLossModel::MaxRange", DoubleValue (4));

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (2);
  NodeContainer wifiApNode;
  wifiApNode.Create (2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  channel.AddPropagationLoss ("ns3::RangePropagationLossModel"); //wireless range limited to 5 meters!

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("HtMcs0"), "ControlMode", StringValue ("HtMcs0"));
  WifiMacHelper mac;

  Ssid ssid = Ssid ("simple-mpdu-aggregation");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "BE_MaxAmpduSize", UintegerValue (maxAmpduSize));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "EnableBeaconJitter", BooleanValue (false),
               "BE_MaxAmpduSize", UintegerValue (maxAmpduSize));

  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, wifiApNode);

  // Setting mobility model
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();

  // AP is between the two stations, each station being located at 5 meters from the AP.
  // The distance between the two stations is thus equal to 10 meters.
  // Since the wireless range is limited to 5 meters, the two stations are hidden from each other.
  /*positionAlloc->Add (Vector (5.0, 0.0, 0.0));
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (10.0, 0.0, 0.0));*/
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (12.0, 0.0, 0.0));
  positionAlloc->Add (Vector (4, 0.0, 0.0));
  positionAlloc->Add (Vector (8, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install (wifiApNode);
  mobility.Install (wifiStaNodes);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");

  Ipv4InterfaceContainer ApInterface;
  ApInterface = address.Assign (apDevice);

  Ipv4InterfaceContainer StaInterface;
  StaInterface = address.Assign (staDevices);

  // Setting applications

/*ApplicationContainer cbrApps;
  uint16_t cbrPort = 9;
//  uint16_t cbrPort2 = 10;
  ApplicationContainer cbrApps2;


  BulkSendHelper BulkSendHelper1 ("ns3::TcpSocketFactory", InetSocketAddress (ApInterface.GetAddress (0), cbrPort));
  BulkSendHelper1.SetAttribute ("MaxBytes", UintegerValue (maxBytes));

  BulkSendHelper BulkSendHelper2 ("ns3::TcpSocketFactory", InetSocketAddress (ApInterface.GetAddress (1), cbrPort));
  BulkSendHelper2.SetAttribute ("MaxBytes", UintegerValue (maxBytes));

  // flow 1:  node 0 -> node 1
  cbrApps.Add (BulkSendHelper1.Install (wifiStaNodes.Get(0)));
  cbrApps.Start (Seconds (0.0));
  cbrApps.Stop (Seconds (simulationTime));

  // flow 2:  node 2 -> node 1
  cbrApps2.Add (BulkSendHelper2.Install (wifiStaNodes.Get(1)));
  cbrApps2.Start (Seconds (0.0));
  cbrApps2.Stop (Seconds (simulationTime));

  PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny(), cbrPort));
  PacketSinkHelper sink2 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny(), cbrPort));

  ApplicationContainer sinkApps = sink.Install (wifiApNode.Get(0));
  ApplicationContainer sinkApps2 = sink2.Install (wifiApNode.Get(1));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (simulationTime));
  sinkApps2.Start (Seconds (0.0));
  sinkApps2.Stop (Seconds (simulationTime));*/

  /*uint16_t port = 9;
  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (wifiApNode);
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (simulationTime + 1));

  UdpClientHelper client (ApInterface.GetAddress (0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
  client.SetAttribute ("Interval", TimeValue (Time ("0.00002"))); //packets/s
  client.SetAttribute ("PacketSize", UintegerValue (payloadSize));

  // Saturated UDP traffic from stations to AP
  ApplicationContainer clientApp1 = client.Install (wifiStaNodes);
  clientApp1.Start (Seconds (1.0));
  clientApp1.Stop (Seconds (simulationTime + 1));*/

  //Separate sinks with ports 9 and 1o
//  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 9));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 9));//any IP in the sink defined in wifiApNode
  ApplicationContainer apSink1;
  apSink1 = sinkHelper.Install(wifiApNode.Get(0));//Specify the Ap that is receiving all data

//  PacketSinkHelper sinkHelper2 ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 10));
  PacketSinkHelper sinkHelper2 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 10));
  ApplicationContainer apSink2;
  apSink2 = sinkHelper2.Install(wifiApNode.Get(1));

//  OnOffHelper OnOff = OnOffHelper ("ns3::UdpSocketFactory", InetSocketAddress (ApInterface.GetAddress (0), 9));
  OnOffHelper OnOff = OnOffHelper ("ns3::TcpSocketFactory", InetSocketAddress (ApInterface.GetAddress (0), 9));
//  OnOff.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=1]"));
//  OnOff.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=1]"));
  OnOff.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  OnOff.SetAttribute ("DataRate", StringValue ("7Mbps"));

//  OnOffHelper OnOff2 = OnOffHelper ("ns3::UdpSocketFactory", InetSocketAddress (ApInterface.GetAddress (1), 10));
  OnOffHelper OnOff2 = OnOffHelper ("ns3::TcpSocketFactory", InetSocketAddress (ApInterface.GetAddress (1), 10)); // Transmitting to this AP2
//  OnOff2.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=1]"));
//  OnOff2.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=1]"));
  OnOff2.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  OnOff2.SetAttribute ("DataRate", StringValue ("7Mbps"));

  //each sta sends separately to one of the sinks on ports 9 and 10
  ApplicationContainer senderAppSta1;

  senderAppSta1 = OnOff.Install(wifiStaNodes.Get(0));// Who is sending the data (Here is the Station 1)
  //OnOff.SetAttribute ("Remote", AddressValue(InetSocketAddress (ApInterface.GetAddress (0), 10)));
  ApplicationContainer senderAppSta2;
  senderAppSta2 = OnOff2.Install(wifiStaNodes.Get(1));

  phy.EnablePcap ("SimpleHtHiddenStations_Ap", apDevice.Get (0));
  phy.EnablePcap ("SimpleHtHiddenStations_Ap2", apDevice.Get (1));
  phy.EnablePcap ("SimpleHtHiddenStations_Sta1", staDevices.Get (0));
  phy.EnablePcap ("SimpleHtHiddenStations_Sta2", staDevices.Get (1));

  AsciiTraceHelper ascii;
  phy.EnableAsciiAll (ascii.CreateFileStream ("SimpleHtHiddenStations.tr"));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime + 1));

  Simulator::Run ();

  //uint64_t totalPacketsThrough = DynamicCast<UdpServer> (serverApp.Get (0))->GetReceived ();

  Simulator::Destroy ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      // first 2 FlowIds are for ECHO apps, we don't want to display them
      //
      // Duration for throughput measurement is 9.0 seconds, since
      //   StartTime of the OnOffApplication is at about "second 1"
      // and
      //   Simulator::Stops at "second 10".
      //if (i->first > 2)
        {
          Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
          std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
          std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
          std::cout << "  TxOffered:  " << i->second.txBytes * 8.0 / 9.0 / 1000 / 1000  / (simulationTime) << " Mbps\n";
          std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
          std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
          std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / 9.0 / 1000 / 1000  / (simulationTime) << " Mbps\n";
        }
    }
//  uint64_t totalPacketsThrough = DynamicCast<OnOffApplication> (node->GetApplication(0))->AssignStreams ();
//  double throughput = totalPacketsThrough * 8 / (simulationTime * 1000000.0);

//  uint64_t totalPacketsThrough = DynamicCast<PacketSink> (sinkApps.Get (0))->GetTotalRx ();
//  uint64_t totalPacketsThrough = DynamicCast<PacketSink> (apSink1.Get (0))->GetTotalRx ();
//  uint64_t totalPacketsThrough = DynamicCast<UdpServer> (apSink1.Get (0))->GetReceived ();
//  double throughput = totalPacketsThrough * 8 / (simulationTime * 1000000.0);

//  uint64_t totalPacketsThrough2 = DynamicCast<PacketSink> (sinkApps2.Get (0))->GetTotalRx ();
//  uint64_t totalPacketsThrough2 = DynamicCast<PacketSink> (apSink2.Get (1))->GetTotalRx ();
//  uint64_t totalPacketsThrough2 = DynamicCast<UdpServer> (apSink2.Get (1))->GetReceived ();
//  double throughput2 = totalPacketsThrough2 * 8 / (simulationTime * 1000000.0);
//  uint64_t totalPacketsThrough = DynamicCast<UdpServer> (serverApp.Get (0))->GetReceived ();
//  double throughput = totalPacketsThrough * payloadSize * 8 / (simulationTime * 1000000.0);


//  uint64_t totalPacketsThrough2 = DynamicCast<UdpServer> (serverApp.Get (1))->GetReceived ();
//  double throughput2 = totalPacketsThrough2 * payloadSize * 8 / (simulationTime * 1000000.0);

//  std::cout << "Throughput: " << throughput << " Mbit/s" << '\n';
//  std::cout << "Throughput2: " << throughput2 << " Mbit/s" << '\n';
/*  if (throughput < minExpectedThroughput || (maxExpectedThroughput > 0 && throughput > maxExpectedThroughput))
    {
      NS_LOG_ERROR ("Obtained throughput " << throughput << " is not in the expected boundaries!");
      exit (1);
    }*/
  return 0;
}
