#include "add_crc_impl.h"
#include <gnuradio/io_signature.h>
// Fix for libboost > 1.75
#include <boost/bind/placeholders.hpp>

using namespace boost::placeholders;
namespace gr {
namespace lora_sdr {

add_crc::sptr add_crc::make(bool has_crc) {
  return gnuradio::get_initial_sptr(new add_crc_impl(has_crc));
}

/**
 * @brief Construct a new add crc impl object
 *
 * @param has_crc : boolean to indicate if crc should be added to the payload
 */
add_crc_impl::add_crc_impl(bool has_crc)
    : gr::block("add_crc", gr::io_signature::make(1, 1, sizeof(uint8_t)),
                gr::io_signature::make(1, 1, sizeof(uint8_t))) {
  m_has_crc = has_crc;

  message_port_register_in(pmt::mp("msg"));
  set_msg_handler(pmt::mp("msg"),
                  boost::bind(&add_crc_impl::msg_handler, this, _1));
  set_tag_propagation_policy(TPP_ALL_TO_ALL);
}

/**
 * @brief Destroy the add crc impl object
 *
 */
add_crc_impl::~add_crc_impl() {}

/**
 * @brief Gnuradio standard function to tell the system it should operate once
 * it has a input item
 *
 * @param noutput_items : number of output items
 * @param ninput_items_required : number of required input items
 */
void add_crc_impl::forecast(int noutput_items,
                            gr_vector_int &ninput_items_required) {
  ninput_items_required[0] = 1;
}

/**
 * @brief CRC16, add 16 bit CRC to the payload.
 *
 * @param crcValue :
 * @param newByte :
 * @return unsigned int
 */
unsigned int add_crc_impl::crc16(unsigned int crcValue, unsigned char newByte) {
  unsigned char i;
  for (i = 0; i < 8; i++) {

    if (((crcValue & 0x8000) >> 8) ^ (newByte & 0x80)) {
      crcValue = (crcValue << 1) ^ 0x1021;
    } else {
      crcValue = (crcValue << 1);
    }
    newByte <<= 1;
  }
  return crcValue;
}

/**
 * @brief Message handler, handles the pmt input message (i.e. input data)
 *
 * @param message : input message (i.e input data)
 */
void add_crc_impl::msg_handler(pmt::pmt_t message) {
  // get input message parse to string and copy into payload variable
  std::string str = pmt::symbol_to_string(message);
  std::copy(str.begin(), str.end(), std::back_inserter(m_payload));
}

/**
 * @brief Main function of the add_crc module, this module will add Cyclic
 * Redundancy Check (CRC) to the payload to be able to detect more bit errors.
 * If m_has_crc is set to True
 *
 * @param noutput_items : number of output items
 * @param ninput_items : number of input items
 * @param input_items  : vector containing the input items
 * @param output_items : vector containting the output items
 * @return int
 */
int add_crc_impl::general_work(int noutput_items, gr_vector_int &ninput_items,
                               gr_vector_const_void_star &input_items,
                               gr_vector_void_star &output_items) {

  const uint8_t *in = (const uint8_t *)input_items[0];
  uint8_t *out = (uint8_t *)output_items[0];
  memcpy(out, in, ninput_items[0] * sizeof(uint8_t));

  // if m_has_crc is True append the CRC to the payload
  if (m_has_crc) {
    uint16_t crc = 0x0000;
    m_payload_len = m_payload.size();
    // calculate CRC on the N-2 firsts data bytes using Poly=1021 Init=0000
    for (int i = 0; i < (int)m_payload_len - 2; i++)
      crc = crc16(crc, m_payload[i]);

    // XOR the obtained CRC with the last 2 data bytes
    crc = crc ^ m_payload[m_payload_len - 1] ^
          (m_payload[m_payload_len - 2] << 8);

    out[ninput_items[0]] = ((crc & 0x000F));
    out[ninput_items[0] + 1] = ((crc & 0x00F0) >> 4);
    out[ninput_items[0] + 2] = ((crc & 0x0F00) >> 8);
    out[ninput_items[0] + 3] = ((crc & 0xF000) >> 12);

    noutput_items = ninput_items[0] + 4;
  }
  // copy input to output
  else {
    noutput_items = ninput_items[0];
  }
  consume_each(ninput_items[0]);
  m_payload.clear();
  return noutput_items;
}
} // namespace lora_sdr
} /* namespace gr */
