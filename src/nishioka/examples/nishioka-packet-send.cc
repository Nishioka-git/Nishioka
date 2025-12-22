/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Simple example demonstrating packet creation and transmission
 * using the nishioka header module
 */

 #include "ns3/core-module.h"
 #include "ns3/network-module.h"
 #include "ns3/log.h"
 
 #include "ns3/nishioka-header.h"
 
 #include <iostream>
 
 using namespace ns3;
 using namespace ns3::nishioka;
 
 NS_LOG_COMPONENT_DEFINE("NishiokaPacketSend");
 
 /**
  * Function to demonstrate packet creation and header manipulation
  */
 void
 CreateAndSendPacket()
 {
     NS_LOG_INFO("Creating a custom packet with header...");
 
     // Create a packet with 50 bytes of payload
     Ptr<Packet> packet = Create<Packet>(50);
 
     // Create and configure the custom header
     NishiokaHeader header;
     header.SetFrameType(NishiokaHeader::CUSTOM_DATA);
     header.SetSeqNum(1);
 
     // Set source address (16-bit short address)
     Mac16Address srcAddr = Mac16Address("00:01");
     header.SetSrcAddrFields(0x1234, srcAddr);
 
     // Set destination address (16-bit short address)
     Mac16Address dstAddr = Mac16Address("00:02");
     header.SetDstAddrFields(0x1234, dstAddr);
 
     // Add the header to the packet
     packet->AddHeader(header);
 
     NS_LOG_INFO("Packet created:");
     NS_LOG_INFO("  Size: " << packet->GetSize() << " bytes");
     NS_LOG_INFO("  Header size: " << header.GetSerializedSize() << " bytes");
     NS_LOG_INFO("  Payload size: " << (packet->GetSize() - header.GetSerializedSize())
                                     << " bytes");
 
     // Print header information
     std::ostringstream oss;
     header.Print(oss);
     NS_LOG_INFO("  Header: " << oss.str());
 
     // Demonstrate deserialization
     NS_LOG_INFO("\nDemonstrating packet deserialization...");
     Ptr<Packet> receivedPacket = packet->Copy();
 
     NishiokaHeader receivedHeader;
     receivedHeader.SetDstAddrMode(NishiokaHeader::SHORTADDR);
     receivedHeader.SetSrcAddrMode(NishiokaHeader::SHORTADDR);
     receivedPacket->RemoveHeader(receivedHeader);
 
     NS_LOG_INFO("Received packet header:");
     std::ostringstream oss2;
     receivedHeader.Print(oss2);
     NS_LOG_INFO("  " << oss2.str());
     NS_LOG_INFO("  Frame Type: " << static_cast<uint32_t>(receivedHeader.GetFrameType()));
     NS_LOG_INFO("  Sequence Number: " << static_cast<uint32_t>(receivedHeader.GetSeqNum()));
     NS_LOG_INFO("  Source Address: " << receivedHeader.GetShortSrcAddr());
     NS_LOG_INFO("  Destination Address: " << receivedHeader.GetShortDstAddr());
 }
 
 /**
  * Function to demonstrate multiple packet creation
  */
 void
 CreateMultiplePackets()
 {
     NS_LOG_INFO("\n=== Creating multiple packets ===");
 
     for (uint8_t i = 0; i < 5; i++)
     {
         Ptr<Packet> packet = Create<Packet>(100 + i * 10);
 
         NishiokaHeader header;
         header.SetFrameType(NishiokaHeader::CUSTOM_DATA);
         header.SetSeqNum(i + 1);
 
         Mac16Address srcAddr = Mac16Address("00:01");
         Mac16Address dstAddr = Mac16Address("00:02");
         header.SetSrcAddrFields(0x1234, srcAddr);
         header.SetDstAddrFields(0x1234, dstAddr);
 
         packet->AddHeader(header);
 
         NS_LOG_INFO("Packet " << static_cast<uint32_t>(i + 1) << ": Size = " << packet->GetSize()
                               << " bytes, SeqNum = "
                               << static_cast<uint32_t>(header.GetSeqNum()));
     }
 }
 
 int
 main(int argc, char* argv[])
 {
     bool verbose = false;
 
     CommandLine cmd(__FILE__);
     cmd.AddValue("verbose", "Enable verbose logging", verbose);
     cmd.Parse(argc, argv);
 
     if (verbose)
     {
         LogComponentEnable("NishiokaPacketSend", LOG_LEVEL_ALL);
         LogComponentEnable("NishiokaHeader", LOG_LEVEL_ALL);
     }
     else
     {
         LogComponentEnable("NishiokaPacketSend", LOG_LEVEL_INFO);
     }
 
     NS_LOG_INFO("=== Nishioka Module Packet Send Example ===");
     NS_LOG_INFO("This example demonstrates:");
     NS_LOG_INFO("  1. Creating packets with custom headers");
     NS_LOG_INFO("  2. Setting header fields");
     NS_LOG_INFO("  3. Serializing and deserializing headers");
     NS_LOG_INFO("");
 
     // Enable packet metadata for debugging
     PacketMetadata::Enable();
 
     // Create and send a single packet
     CreateAndSendPacket();
 
     // Create multiple packets
     CreateMultiplePackets();
 
     NS_LOG_INFO("\n=== Example completed successfully ===");
 
     return 0;
 }
 
 