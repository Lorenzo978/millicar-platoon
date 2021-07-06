/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
*   Copyright (c) 2020 University of Padova, Dep. of Information Engineering,
*   SIGNET lab.
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License version 2 as
*   published by the Free Software Foundation;
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "ns3/mmwave-sidelink-spectrum-phy.h"
#include "ns3/mmwave-vehicular-net-device.h"
#include "ns3/mmwave-vehicular-helper.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/mobility-module.h"
#include "ns3/config.h"
#include "ns3/isotropic-antenna-model.h"
#include "ns3/spectrum-helper.h"
#include "ns3/mmwave-spectrum-value-helper.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/core-module.h"
#include "string.h"

NS_LOG_COMPONENT_DEFINE ("ThreeVehiclePlatoon");

using namespace ns3;
using namespace millicar;

uint32_t g_rxPackets; // total number of received packets
uint32_t g_txPackets; // total number of transmitted packets
uint32_t packetSentCounter;
double lastAverageRTT;
double averageRTT;


uint32_t nodeNumber = 5;
uint32_t packetSize = 1200; // UDP packet size in bytes
uint32_t startTime = 1000; // application start time in milliseconds
uint32_t endTime = 4300; // application end time in milliseconds
double interPacketInterval = 33; // interpacket interval in milliseconds
double intraGroupDistance = 5; // distance between two vehicles belonging to the same group
const std::string channelCondition = "n";
const std::string scenario = "V2V-Urban";

Time g_firstReceived; // timestamp of the first time a packet is received
Time g_lastReceived; // timestamp of the last received packet

static void Rx (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> p)
{
  g_rxPackets++;
  SeqTsHeader header;

  p->PeekHeader(header);
  packetSentCounter++;

  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << packetSentCounter << "\t" << header.GetSeq() << "\t" << header.GetTs().GetSeconds() << std::endl;
  
  // Calcolo l'avarage RTT per l'ultimo veicolo del platoon
  if(packetSentCounter == nodeNumber-1)
  {
    packetSentCounter = 0;
    lastAverageRTT = lastAverageRTT + Simulator::Now ().GetSeconds ()-header.GetTs().GetSeconds();
  }

  
  NS_LOG_DEBUG(Simulator::Now ().GetSeconds () << "\t" << header.GetSeq());
  averageRTT = averageRTT + Simulator::Now ().GetSeconds ()-header.GetTs().GetSeconds();

  if (g_rxPackets > 1)
  {

    g_lastReceived = Simulator::Now();
  }
  else
  {
    g_firstReceived = Simulator::Now();
  }
}

int main (int argc, char *argv[])
{
  LogComponentEnable("ThreeVehiclePlatoon", LOG_LEVEL_DEBUG);
  std::cout << "----------- Start -----------" << std::endl;

  // system parameters
  double bandwidth = 1e8; // bandwidth in Hz
  double frequency = 28e9; // the carrier frequency
  uint32_t numerology = 3; // the numerology

  // mobility
  double speed = 27.7; // speed of the vehicles m/s
  uint32_t ct;
  double totalPacket = ((endTime-startTime)/interPacketInterval) * (nodeNumber-1);
  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("scratch/test.txt");

  CommandLine cmd;
  //
  cmd.AddValue ("bandwidth", "used bandwidth", bandwidth);
  cmd.AddValue ("iip", "inter packet interval, in microseconds", interPacketInterval);
  cmd.AddValue ("intraGroupDistance", "distance between two vehicles belonging to the same group, y-coord", intraGroupDistance);
  cmd.AddValue ("numerology", "set the numerology to use at the physical layer", numerology);
  cmd.AddValue ("frequency", "set the carrier frequency", frequency);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::MmWaveSidelinkMac::UseAmc", BooleanValue (true));
  Config::SetDefault ("ns3::MmWavePhyMacCommon::CenterFreq", DoubleValue (frequency));
  Config::SetDefault ("ns3::MmWaveVehicularHelper::Bandwidth", DoubleValue (bandwidth));
  Config::SetDefault ("ns3::MmWaveVehicularHelper::Numerology", UintegerValue (numerology));
  Config::SetDefault ("ns3::MmWaveVehicularPropagationLossModel::ChannelCondition", StringValue(channelCondition));
  Config::SetDefault ("ns3::MmWaveVehicularPropagationLossModel::Scenario", StringValue(scenario));
  Config::SetDefault ("ns3::MmWaveVehicularPropagationLossModel::Shadowing", BooleanValue (true));
  Config::SetDefault ("ns3::MmWaveVehicularSpectrumPropagationLossModel::UpdatePeriod", TimeValue (MilliSeconds (1)));
  Config::SetDefault ("ns3::MmWaveVehicularAntennaArrayModel::AntennaElements", UintegerValue (16));
  Config::SetDefault ("ns3::MmWaveVehicularAntennaArrayModel::AntennaElementPattern", StringValue ("3GPP-V2V"));
  Config::SetDefault ("ns3::MmWaveVehicularAntennaArrayModel::IsotropicAntennaElements", BooleanValue (true));
  Config::SetDefault ("ns3::MmWaveVehicularAntennaArrayModel::NumSectors", UintegerValue (2));

  Config::SetDefault ("ns3::MmWaveVehicularNetDevice::RlcType", StringValue("LteRlcUm"));
  Config::SetDefault ("ns3::MmWaveVehicularHelper::SchedulingPatternOption", EnumValue(2)); // use 2 for SchedulingPatternOption=OPTIMIZED, 1 or SchedulingPatternOption=DEFAULT
  Config::SetDefault ("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue (500*1024));

  // create the nodes
  NodeContainer n;
  n.Create (nodeNumber);
  // create the mobility models
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (n);

  for(ct = 0; ct < nodeNumber; ct++)
  {
    n.Get (ct)->GetObject<MobilityModel> ()->SetPosition (Vector (0,intraGroupDistance*(nodeNumber-ct),0));
    n.Get (ct)->GetObject<ConstantVelocityMobilityModel> ()->SetVelocity (Vector (0, speed, 0));
  }

  // create and configure the helper
  Ptr<MmWaveVehicularHelper> helper = CreateObject<MmWaveVehicularHelper> ();
  helper->SetNumerology (3);
  helper->SetPropagationLossModelType ("ns3::MmWaveVehicularPropagationLossModel");
  helper->SetSpectrumPropagationLossModelType ("ns3::MmWaveVehicularSpectrumPropagationLossModel");
  NetDeviceContainer devs = helper->InstallMmWaveVehicularNetDevices (n);

  // Install the TCP/IP stack in the two nodes

  InternetStackHelper internet;
  internet.Install (n);

  Ipv4AddressHelper ipv4;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devs);

  // Need to pair the devices in order to create a correspondence between transmitter and receiver
  // and to populate the < IP addr, RNTI > map.
  helper->PairDevices(devs);

  // Set the routing table
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> staticRouting = ipv4RoutingHelper.GetStaticRouting (n.Get (0)->GetObject<Ipv4> ());
  staticRouting->SetDefaultRoute (n.Get (1)->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal () , 2 );

  Ptr<mmwave::MmWaveAmc> m_amc = CreateObject <mmwave::MmWaveAmc> (helper->GetConfigurationParameters());

  // setup the applications
  Config::SetDefault ("ns3::UdpClient::MaxPackets", UintegerValue (0xFFFFFFFF));
  Config::SetDefault ("ns3::UdpClient::Interval", TimeValue (MilliSeconds (interPacketInterval)));
  Config::SetDefault ("ns3::UdpClient::PacketSize", UintegerValue (packetSize));

  // create the applications
  uint32_t port = 4000;

  UdpEchoServerHelper server (port);
  ApplicationContainer echoApps = server.Install (n.Get (0));
  echoApps.Start (Seconds (0.0));


  echoApps.Get(0)->TraceConnectWithoutContext ("Rx", MakeBoundCallback (&Rx, stream));

  UdpClientHelper client (n.Get (0)->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal (), port);

  for(ct = 1; ct < nodeNumber; ct++)
  {
    ApplicationContainer a = client.Install (n.Get (ct));
    a.Start (MilliSeconds (startTime));
    a.Stop (MilliSeconds (endTime));
  }

  Simulator::Stop(MilliSeconds (endTime + 100));
  Simulator::Run ();
  Simulator::Destroy ();

  std::cout << "----------- Statistics -----------" << std::endl;
  std::cout << "Number of vehicles:\t" << nodeNumber << std::endl;
  std::cout << "Packets size:\t\t" << packetSize << " Bytes" << std::endl;
  std::cout << "Duration:\t\t" << endTime-startTime << std::endl;
  std::cout << "Packets received:\t" << g_rxPackets << std::endl;
  std::cout << "Total packet sent:\t" << totalPacket << std::endl;
  std::cout << "Packet rate loss:\t" << ((totalPacket-g_rxPackets)/totalPacket)*100 << "%" << std::endl;
  std::cout << "Average Throughput:\t" << (double(g_rxPackets)*(double(packetSize)*8)/double( g_lastReceived.GetSeconds() - g_firstReceived.GetSeconds()))/(1e6 * (nodeNumber-1)) << " Mbps" << std::endl;
  std::cout << "Average RTT:\t\t" << averageRTT/g_rxPackets << std::endl;
  std::cout << "Average last RTT:\t" << lastAverageRTT/(g_rxPackets/(nodeNumber-1)) << std::endl;
  return 0;
}
