#include "hb/packet.hpp"
#include "hb/protocol.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

static std::array<std::uint8_t,HB_PACKET_BYTES> make_packet(std::uint32_t seq) {
    hb_ads1299_sample_t sample{};
    sample.timestamp_ms=123456u+seq; sample.status24=0xC01234u;
    sample.channel[0]=0x123456; sample.channel[1]=-2;
    for (std::size_t i=2;i<8;i++) sample.channel[i]=static_cast<std::int32_t>(i*17);
    std::array<std::uint8_t,HB_PACKET_BYTES> bytes{};
    assert(hb_encode_sample_packet(&sample,seq,bytes.data(),bytes.size())==HB_PACKET_BYTES);
    return bytes;
}
static std::array<std::uint8_t,HB_DUAL_PACKET_BYTES> make_dual_packet(
    std::uint32_t seq, std::uint32_t timestamp_us) {
    hb_dual_ads1299_sample_t sample{};
    sample.timestamp_us=timestamp_us;
    sample.status24[0]=0x123456u;
    sample.status24[1]=0xABCDEFu;
    for (std::size_t i=0;i<16;i++) sample.channel[i]=static_cast<std::int32_t>(i*101)-700;
    std::array<std::uint8_t,HB_DUAL_PACKET_BYTES> bytes{};
    assert(hb_encode_dual_sample_packet(&sample,seq,bytes.data(),bytes.size())==HB_DUAL_PACKET_BYTES);
    return bytes;
}
int main() {
    const auto one=make_packet(41), two=make_packet(42);
    hb::SamplePacket decoded{};
    assert(hb::decode_packet(one.data(),one.size(),decoded));
    assert(decoded.version==1u && decoded.channel_count==8u);
    assert(decoded.sequence==41 && decoded.timestamp_ms==123497u);
    assert(decoded.status24==0xC01234u);
    assert(decoded.channel[0]==0x123456 && decoded.channel[1]==-2);

    hb::StreamParser parser;
    std::vector<std::uint8_t> bytes={'x','y','H','B'};
    bytes.insert(bytes.end(),one.begin(),one.end());
    bytes.insert(bytes.end(),two.begin(),two.end());
    auto first=parser.feed(bytes.data(),17);
    assert(first.empty());
    auto second=parser.feed(bytes.data()+17,bytes.size()-17);
    assert(second.size()==2 && second[0].sequence==41 && second[1].sequence==42);

    auto damaged=make_packet(43); damaged[21]^=0x01;
    auto third=parser.feed(damaged.data(),damaged.size());
    assert(third.empty() && parser.rejected_frames()>=1);
    const auto valid=make_packet(44);
    auto recovered=parser.feed(valid.data(),valid.size());
    assert(recovered.size()==1 && recovered[0].sequence==44);

    const auto dual_one=make_dual_packet(100u,500000u);
    const auto dual_two=make_dual_packet(101u,501000u);
    hb::SamplePacket dual_decoded{};
    assert(hb::decode_packet(dual_one.data(),dual_one.size(),dual_decoded));
    assert(dual_decoded.version==2u && dual_decoded.channel_count==16u);
    assert(dual_decoded.timestamp_us==500000u && dual_decoded.timestamp_ms==500u);
    assert(dual_decoded.status24==0x123456u && dual_decoded.status24_secondary==0xABCDEFu);
    assert(dual_decoded.channel[0]==-700 && dual_decoded.channel[15]==815);
    std::vector<std::uint8_t> mixed;
    mixed.insert(mixed.end(),one.begin(),one.end());
    mixed.insert(mixed.end(),dual_one.begin(),dual_one.end());
    mixed.insert(mixed.end(),dual_two.begin(),dual_two.end());
    auto mixed_packets=parser.feed(mixed.data(),mixed.size());
    assert(mixed_packets.size()==3u && mixed_packets[0].channel_count==8u);
    assert(mixed_packets[1].channel_count==16u && mixed_packets[2].sequence==101u);

    hb::SampleRateMonitor rate;
    for (std::uint32_t i=0;i<1001u;i++) {
        auto bytes1000=make_dual_packet(2000u+i,1000000u+i*1000u);
        hb::SamplePacket s{};
        assert(hb::decode_packet(bytes1000.data(),bytes1000.size(),s));
        assert(rate.observe(s));
    }
    assert(rate.sample_count()==1001u && rate.sequence_gaps()==0u);
    assert(rate.measured_hz()>999.9 && rate.measured_hz()<1000.1);
    hb::SamplePacket one_rate{};
    one_rate.version=1u; one_rate.channel_count=8u;
    assert(!rate.observe(one_rate));

    auto bad_dual=dual_one; bad_dual[31]^=0x80u;
    assert(!hb::decode_packet(bad_dual.data(),bad_dual.size(),dual_decoded));

    hb_ads1299_sample_t out_of_range{}; out_of_range.channel[0]=8388608;
    std::array<std::uint8_t,HB_PACKET_BYTES> out{};
    assert(hb_encode_sample_packet(&out_of_range,0,out.data(),out.size())==0);
    std::cout << "HBCI protocol tests passed\n";
    return 0;
}
