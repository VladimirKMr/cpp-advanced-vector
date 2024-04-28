#pragma once

#include <cassert>
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept {
        buffer_ = nullptr;
        capacity_ = 0;
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            Deallocate(buffer_);
            capacity_ = 0;
            Swap(rhs);
        }
        return *this;
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size_);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept {
        Swap(other);
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    using iterator = T*;
    using const_iterator = const T*;
    
    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator cbegin() const noexcept {
        return begin();
    }
    const_iterator cend() const noexcept {
        return end();
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                // copy-and-swap
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            } else {
                // копируем из rhs, создав при необходимости новые, или удалив существующие
                if (rhs.size_ < size_) {
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + rhs.size_, data_.GetAddress());
                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                } else {
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + size_, data_.GetAddress());
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }
    
    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {
        if (new_size <= size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        } else {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        size_ = new_size;
    }

    void PopBack() {
        if (size_ > 0) {
            --size_;
            std::destroy_at(data_.GetAddress() + size_);
        }
    }

    // реализация EmplaceBack на основе Emplace
    // при добавлении этого метода не проходит тесты в тренажере (Вы неверно реализовали метод EmplaceBack)
    // прошу сообщить в чем ошибка, или тесты на это не расчитаны? 
    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        T* value = Emplace(end(), std::forward<Args>(args)...);
        return *value;
    }

    template <typename Type>
    void PushBack(Type&& value) {
        EmplaceBack(std::forward<Type>(value));
    }

    // реализация PushBack c константной ссылкой, пробовал по разному, но 
    // при добавлении этого метода не проходит тесты в тренажере (Вы неверно реализовали метод PushBack)
    // прошу сообщить в чем ошибка, или тесты на это не расчитаны? 
    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&& ...args) {
        assert(pos >= begin() && pos <= end());
        size_t new_capacity(size_ == 0 ? 1 : size_ * 2);
        size_t pos_num = pos - begin();
        iterator value = nullptr;

        if (size_ == Capacity()) {
            RawMemory<T> new_data(new_capacity);
            value = new (new_data + pos_num) T(std::forward<Args>(args)...);

            ReAllocate(pos_num, new_data);

            std::destroy_n(begin(), size_);
            data_.Swap(new_data);

        } else {
            if (size_ != 0) {
                new (data_ + size_) T(std::forward<T>(*(end() - 1)));
                try {
                    std::move_backward(begin() + pos_num, end(), end() + 1);
                }
                catch (...) {
                    std::destroy_at(end());
                    throw;
                }
                std::destroy_at(begin() + pos_num);
            }
            value = new (data_ + pos_num) T(std::forward<Args>(args)...);
        }
        ++size_;
        
        return value;
    }

    iterator Erase(const_iterator pos) {
        assert(pos >= begin() && pos < end());
        size_t pos_num = pos - begin();
        std::move(begin() + pos_num + 1, end(), begin() + pos_num);
        PopBack();
        return begin() + pos_num;
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    // реализация для ссылки
    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

private:
    void ReAllocate(size_t pos_num, RawMemory<T>& new_data) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(begin(), pos_num, new_data.GetAddress());
                std::uninitialized_move_n(begin() + pos_num, size_ - pos_num, new_data.GetAddress() + pos_num + 1);
            } else {
                try {
                    std::uninitialized_copy_n(begin(), pos_num, new_data.GetAddress());
                    std::uninitialized_copy_n(begin() + pos_num, size_ - pos_num, new_data.GetAddress() + pos_num + 1);
                }
                catch (...) {
                    std::destroy_at(new_data.GetAddress() + pos_num);
                    throw;
                }
            }
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};