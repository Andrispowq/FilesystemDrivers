#ifndef BITMAP_H
#define BITMAP_H

#include <stdint.h>

#define INDEX_FROM_BIT(a) (a / (8 * 8))
#define OFFSET_FROM_BIT(a) (a % (8 * 8))

namespace exFAT
{
    class Bitmap
    {
    public:
        bool Get(uint64_t index)
        {
            uint64_t idx = INDEX_FROM_BIT(index);
            uint64_t off = OFFSET_FROM_BIT(index);
            return (buffer[idx] & (0x1 << off));
        }

        void Set(uint64_t index, bool value)
        {
            uint64_t idx = INDEX_FROM_BIT(index);
            uint64_t off = OFFSET_FROM_BIT(index);

            if (value)
            {
                buffer[idx] |= (0x1 << off);
            }
            else
            {
                buffer[idx] &= ~(0x1 << off);

                if (index < lastFree)
                {
                    lastFree = index;
                }
            }
        }

        uint64_t First()
        {
            uint64_t i = 0;
            uint64_t j = 0;

            if (lastFree != 0xFFFFFFFFFFFFFFFF)
            {
                i = INDEX_FROM_BIT(lastFree);
            }

            for (; i < size; i++)
            {
                if (buffer[i] != 0xFFFFFFFFFFFFFFFF)
                {
                    for (j = 0; j < 64; j++)
                    {
                        uint64_t toTest = 0x1 << j;
                        if (!(buffer[i] & toTest))
                        {
                            return i * 8 * 8 + j;
                        }
                    }
                }
            }

            return 0xFFFFFFFFFFFFFFFF;
        }

        void SetSize(uint64_t size) { this->size = size; }
        void SetBuffer(uint64_t* buffer) { this->buffer = buffer; }

    public:
        uint64_t size = 0;
        uint64_t* buffer = nullptr;
        uint64_t lastFree = 0xFFFFFFFFFFFFFFFF;
	};
}

#endif