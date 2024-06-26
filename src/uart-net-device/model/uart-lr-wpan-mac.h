/*
 * Copyright (c) 2024 Tokushima University, Japan
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
 * Author:
 *  Alberto Gallegos Ramonet <alramonet@is.tokushima-u.ac.jp>
 */

#ifndef UART_LRWPAN_MAC_H
#define UART_LRWPAN_MAC_H

#include <ns3/lr-wpan-mac-base.h>
#include <ns3/traced-callback.h>
#include <ns3/traced-value.h>

#include <boost/asio.hpp>
#include <iostream>
#include <thread>
#include <vector>

namespace ns3
{

class Packet;

namespace uartnetdevice
{

enum ReadState
{
    RX_START = 0,
    RX_PRIMITIVE_TYPE = 1,
    RX_PARAMS_SIZE = 2,
    RX_WAIT_DATA = 3
};

enum PrimitiveType
{
    NONE_CFM = 0,
    SCAN_CFM = 1,
    START_CFM = 2,
    ASSOCIATE_CFM = 3,
    ASSOCIATE_IND = 4,
    COMM_STATUS_IND = 5,
    DATA_CFM = 6,
    DATA_IND = 7,
    SET_CFM = 8,
    GET_CFM = 9,
    ORPHAN_IND = 10
};

class UartLrWpanMac : public lrwpan::LrWpanMacBase
{
  public:
    /**
     * Get the type ID.
     *
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    /**
     * Default constructor.
     */
    UartLrWpanMac();
    UartLrWpanMac(const std::string& port);
    ~UartLrWpanMac() override;

    void McpsDataRequest(lrwpan::McpsDataRequestParams params, Ptr<Packet> p) override;

    void MlmeStartRequest(lrwpan::MlmeStartRequestParams params) override;

    void MlmeScanRequest(lrwpan::MlmeScanRequestParams params) override;

    void MlmeAssociateRequest(lrwpan::MlmeAssociateRequestParams params) override;

    void MlmeAssociateResponse(lrwpan::MlmeAssociateResponseParams params) override;

    void MlmeOrphanResponse(lrwpan::MlmeOrphanResponseParams params) override;

    void MlmeSyncRequest(lrwpan::MlmeSyncRequestParams params) override;

    void MlmePollRequest(lrwpan::MlmePollRequestParams params) override;

    void MlmeSetRequest(lrwpan::MacPibAttributeIdentifier id,
                        Ptr<lrwpan::MacPibAttributes> attribute) override;

    void MlmeGetRequest(lrwpan::MacPibAttributeIdentifier id) override;

  protected:
    // Inherited from Object.
    void DoDispose() override;

  private:
    void Uint8ToBytes(std::vector<uint8_t>& dataArray, uint8_t intValue);
    void Uint16ToBytes(std::vector<uint8_t>& dataArray, uint16_t intValue);
    void Uint32ToBytes(std::vector<uint8_t>& dataArray, uint32_t intValue);
    void Uint64ToBytes(std::vector<uint8_t>& dataArray, uint64_t intValue);

    uint8_t BytesToUint8(const std::vector<uint8_t>& dataArray, uint8_t& pos);
    uint16_t BytesToUint16(const std::vector<uint8_t>& dataArray, uint8_t& pos);
    uint32_t BytesToUint32(const std::vector<uint8_t>& dataArray, uint8_t& pos);
    uint64_t BytesToUint64(const std::vector<uint8_t>& dataArray, uint8_t& pos);

    void OpenPort();
    void ReadByte();
    void RunIoService();

    void ProcessData();

    void ScanConfirm();
    void StartConfirm();
    void AssociateIndication();
    void CommStatusIndication();
    void AssociateConfirm();
    void DataConfirm();
    void DataIndication();
    void SetConfirm();
    void GetConfirm();
    void OrphanIndication();

    boost::asio::io_service m_ioService;
    boost::asio::serial_port m_serial;
    std::string m_port;
    std::thread m_ioServiceThread;
    std::mutex mutex_;

    ReadState m_rxState;
    PrimitiveType m_rxPrimitiveType;
    uint32_t m_paramsMaxSize;
    std::vector<uint8_t> m_rxData;

    uint32_t m_rxByteCount;
};

} // namespace uartnetdevice
} // namespace ns3

#endif /* UART_LRWPAN_MAC_H */
