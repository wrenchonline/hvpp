#pragma once
#include <cstdint>

class device
{
  public:
    void initialize() noexcept;
    void destroy() noexcept;

  public:
    virtual void on_create() noexcept {}
    virtual void on_close() noexcept {}
    virtual void on_ioctl(uint32_t code, void* buffer, size_t buffer_size) noexcept
    { (void)code; (void)buffer; (void)buffer_size; }

  private:
    void* device_object_;
};
