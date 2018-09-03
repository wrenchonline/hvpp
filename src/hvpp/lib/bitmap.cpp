#include "bitmap.h"

void* bitmap::buffer() noexcept
{
  return buffer_;
}

const void* bitmap::buffer() const noexcept
{
  return buffer_;
}

int bitmap::size_in_bits() const noexcept
{
  return size_in_bits_;
}

int bitmap::size_in_bytes() const noexcept
{
  return size_in_bits_ / 8;
}

void bitmap::set(int index, int count) noexcept
{
  //
  // Stolen from ReactOS - RtlSetbits.
  //

  word_t* buffer = &buffer_[word(index)];
  int bits = offset(index);

  if (bits)
  {
    bits = bit_count - bits;

    if (count < bits)
    {
      bits -= count;
      *buffer |= (~mask(offset(index)) + 1) << bits >> bits;
      return;
    }

    *buffer |= ~mask(offset(index)) + 1;
    buffer++;
    count -= bits;
  }

  memset(buffer, ~0, count >> 3);
  buffer += count / bit_count;

  count %= bit_count;
  if (count != 0)
  {
    *buffer |= mask(count) - 1;
  }
}

void bitmap::set(int bit) noexcept
{
  buffer_[word(bit)] |= mask(bit);
}

void bitmap::set() noexcept
{
  memset(buffer_, ~0, size_in_bits_ / 8);
}

void bitmap::clear(int index, int count) noexcept
{
  //
  // Stolen from ReactOS - RtlClearBits.
  //

  word_t* buffer = &buffer_[word(index)];
  int bits = offset(index);

  if (bits)
  {
    bits = bit_count - bits;

    if (count < bits)
    {
      bits -= count;
      *buffer &= ~((~mask(offset(index)) + 1) << bits >> bits);
      return;
    }

    *buffer &= mask(offset(index)) - 1;
    buffer++;
    count -= bits;
  }

  memset(buffer, 0, count >> 3);
  buffer += count / bit_count;

  count %= bit_count;
  if (count != 0)
  {
    *buffer &= ~(mask(count) - 1);
  }
}

void bitmap::clear(int bit) noexcept
{
  buffer_[word(bit)] &= ~mask(bit);
}

void bitmap::clear() noexcept
{
  memset(buffer_, 0, size_in_bits_ / 8);
}

bool bitmap::test(int bit) const noexcept
{
  return !!ia32_asm_bt(buffer_, bit);
}

int bitmap::find_first_set(int count) const noexcept
{
  return find_first_set(0, count);
}

int bitmap::find_first_set(int index, int count) const noexcept
{
  if (count > size_in_bits_)
  {
    return -1;
  }

  if (index >= size_in_bits_)
  {
    index = 0;
  }

  if (count == 0)
  {
    return index & ~7;
  }

  int current_bit = index;

  while (current_bit + count <= size_in_bits_)
  {
    static constexpr int max_count = std::numeric_limits<decltype(count)>::max();

    current_bit += get_length_of_clear(current_bit, max_count);
    int current_length = get_length_of_set(current_bit, count);

    if (current_length >= count)
    {
      return current_bit;
    }

    current_bit += current_length;
  }

  return -1;
}

int bitmap::find_first_set() const noexcept
{
  for (int i = 0; i * 8 < size_in_bits_; ++i)
  {
    if (buffer_[i])
    {
      return std::min(
        static_cast<int>(i * 8 + ia32_asm_bsf(static_cast<uint64_t>(buffer_[i]))),
        size_in_bits_);
    }
  }

  return size_in_bits_;
}

int bitmap::find_first_clear(int index, int count) const noexcept
{
  if (count > size_in_bits_)
  {
    return -1;
  }

  if (index >= size_in_bits_)
  {
    index = 0;
  }

  if (count == 0)
  {
    return index & ~7;
  }

  int current_bit = index;

  while (current_bit + count < size_in_bits_)
  {
    static constexpr int max_count = std::numeric_limits<decltype(count)>::max();

    current_bit += get_length_of_set(current_bit, max_count);
    int current_length = get_length_of_clear(current_bit, count);

    if (current_length >= count)
    {
      return current_bit;
    }

    current_bit += current_length;
  }

  return -1;
}

int bitmap::find_first_clear(int count) const noexcept
{
  return find_first_clear(0, count);
}

int bitmap::find_first_clear() const noexcept
{
  for (int i = 0; i * 8 < size_in_bits_; ++i)
  {
    if (buffer_[i] != ~0)
    {
      return std::min(
        static_cast<int>(i * 8 + ia32_asm_bsf(~static_cast<uint64_t>(buffer_[i]))),
        size_in_bits_);
    }
  }

  return size_in_bits_;
}

bool bitmap::are_bits_set(int index, int count) const noexcept
{
  if (index + count > size_in_bits_ ||
    index + count <= index)
  {
    return false;
  }

  return get_length_of_set(index, count) >= count;
}

bool bitmap::are_bits_clear(int index, int count) const noexcept
{
  if (index + count > size_in_bits_ ||
    index + count <= index)
  {
    return false;
  }

  return get_length_of_clear(index, count) >= count;
}

bool bitmap::all_set() const noexcept
{
  return are_bits_set(0, size_in_bits_);
}

bool bitmap::all_clear() const noexcept
{
  return are_bits_clear(0, size_in_bits_);
}

int bitmap::get_length_of_set(int index, int count) const noexcept
{
  //
  // Stolen from ReactOS - RtlpGetLengthOfRunSet.
  //

  if (index >= size_in_bits_)
  {
    return 0;
  }

  const word_t* buffer = &buffer_[word(index)];
  int bit_position = offset(index);

  count = std::min(count, size_in_bits_ - index);
  const word_t* buffer_max = &buffer[word(bit_position + count + bit_count - 1)];

  word_t inv_value = ~(*buffer++) >> bit_position << bit_position;

  while (inv_value == 0 && buffer < buffer_max)
  {
    inv_value = ~(*buffer++);
  }

  if (inv_value == 0)
  {
    return count;
  }

  bit_position = ia32_asm_bsf(inv_value);

  int length = static_cast<int>(buffer - buffer_) * bit_count - index;
  length += bit_position - bit_count;

  if (length > size_in_bits_ - index)
  {
    length = size_in_bits_ - index;
  }

  return length;
}

int bitmap::get_length_of_clear(int index, int count) const noexcept
{
  //
  // Stolen from ReactOS - RtlpGetLengthOfRunClear.
  //

  if (index >= size_in_bits_)
  {
    return 0;
  }

  const word_t* buffer = &buffer_[word(index)];
  int bit_position = offset(index);

  count = std::min(count, size_in_bits_ - index);
  const word_t* buffer_max = &buffer[word(bit_position + count + bit_count - 1)];

  word_t value = *buffer++ >> bit_position << bit_position;

  while (value == 0 && buffer < buffer_max)
  {
    value = *buffer++;
  }

  if (value == 0)
  {
    return count;
  }

  bit_position = ia32_asm_bsf(value);

  int length = static_cast<int>(buffer - buffer_) * bit_count - index;
  length += bit_position - bit_count;

  if (length > size_in_bits_ - index)
  {
    length = size_in_bits_ - index;
  }

  return length;
}
