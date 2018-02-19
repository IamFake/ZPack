#ifndef PACKER_PREPARE_INT_H
#define PACKER_PREPARE_INT_H

#include <type_traits>
#include <typeinfo>
#include "_endianness.h"

extern Endianness endianess;

template<typename T, size_t b>
struct _EndiannessSwap;

template<typename T>
struct _EndiannessSwap<T, 2> {
    T operator()(T &num) {
        std::cout << "NORM spec for type " << typeid(T).hash_code() << std::endl;
        T value = 0;

        value |= (num & 0xFF00) >> 8;
        value |= (num & 0x00FF) << 8;

        return value;
    }
};

template<typename T>
struct _EndiannessSwap<T, 4> {
    T operator()(T &num) {
        T value = 0;

        value |= (num & 0xFF000000) >> 24;
        value |= (num & 0x00FF0000) >> 8;
        value |= (num & 0x0000FF00) << 8;
        value |= (num & 0x000000FF) << 24;

        return value;
    }
};

template<typename T>
struct _EndiannessSwap<T, 8> {
    T operator()(T &num) {
        T value = 0;

        value |= (num & 0xFF00000000000000) >> 56;
        value |= (num & 0x00FF000000000000) >> 40;
        value |= (num & 0x0000FF0000000000) >> 24;
        value |= (num & 0x000000FF00000000) >> 8;
        value |= (num & 0x00000000FF000000) << 8;
        value |= (num & 0x0000000000FF0000) << 24;
        value |= (num & 0x000000000000FF00) << 40;
        value |= (num & 0x00000000000000FF) << 56;

        return value;
    }
};

template<typename T>
T prepareInt(T num) {
    if (endianess == Endianness::BIG && std::is_arithmetic<T>::value) {
        return _EndiannessSwap<T, sizeof(T)>()(num);
    } else {
        return num;
    }
}

template<typename T, size_t b>
struct _IntCharExchange;

template<typename T>
struct _IntCharExchange<T, 2> {
    void operator()(T &num, unsigned char *chr) {
        T e_num = prepareInt(num);

        chr[0] = (e_num & 0x00FF);
        chr[1] = ((e_num >> 8) & 0x00FF);
    }
};

template<typename T>
struct _IntCharExchange<T, 4> {
    void operator()(T &num, unsigned char *chr) {
        T e_num = prepareInt(num);

        chr[0] = (e_num & 0x000000FF);
        chr[1] = ((e_num >> 8) & 0x000000FF);
        chr[2] = ((e_num >> 16) & 0x000000FF);
        chr[3] = ((e_num >> 24) & 0x000000FF);
    }
};

template<typename T>
struct _IntCharExchange<T, 8> {
    void operator()(T &num, unsigned char *chr) {
        T e_num = prepareInt(num);

        chr[0] = (e_num & 0x00000000000000FF);
        chr[1] = ((e_num >> 8) & 0x00000000000000FF);
        chr[2] = ((e_num >> 16) & 0x00000000000000FF);
        chr[3] = ((e_num >> 24) & 0x00000000000000FF);
        chr[4] = ((e_num >> 32) & 0x00000000000000FF);
        chr[5] = ((e_num >> 40) & 0x00000000000000FF);
        chr[6] = ((e_num >> 48) & 0x00000000000000FF);
        chr[7] = ((e_num >> 56) & 0x00000000000000FF);
    }
};

template<typename T>
void assignInt(T num, unsigned char *chr) {
    _IntCharExchange<T, sizeof(T)>()(num, chr);
}

template<typename T, size_t b>
struct _CharIntExchange;

template<typename T>
struct _CharIntExchange<T, 2> {
    T operator()(const unsigned char *chr) {
        T e_num = (T) chr[0] + ((T) chr[1] << 8);
        return prepareInt(e_num);
    }
};

template<typename T>
struct _CharIntExchange<T, 4> {
    T operator()(const unsigned char *chr) {
        T e_num = (T) chr[0] + ((T) chr[1] << 8) + ((T) chr[2] << 16) + ((T) chr[3] << 24);
        return prepareInt(e_num);
    }
};

template<typename T>
struct _CharIntExchange<T, 8> {
    T operator()(const unsigned char *chr) {
        T e_num = (T) chr[0] + ((T) chr[1] << 8) + ((T) chr[2] << 16) + ((T) chr[3] << 24) +
            ((T) chr[4] << 32) + ((T) chr[5] << 40) + ((T) chr[6] << 48) + ((T) chr[7] << 56);
        return prepareInt(e_num);
    }
};

template<typename T>
inline T readInt(const unsigned char *chr) {
    return _CharIntExchange<T, sizeof(T)>()(chr);
}

#endif //PACKER_PREPARE_INT_H
