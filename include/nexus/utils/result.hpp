#pragma once

#include "nexus/common.hpp"
#include "nexus/error.hpp"
#include "nexus/utils/format.hpp"
#include "nexus/utils/result/variant.hpp"

#include <concepts>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace nexus {

/**
 * @brief Check if type is a result
 *
 * @tparam T Target type.
 * @note The concept only checks types defined in target.
 */
template <typename T>
concept IsResult = requires {
    typename T::ValueType;
    typename T::ErrorType;
};

/**
 * @brief A rust-like result type.
 *
 * @tparam T Value type.
 * @tparam E Error type.
 */
template <typename T, typename E>
class Result : public std::variant<Ok<T>, Err<E>> {
  public:
    using ValueType = Ok<T>::ValueType;
    using ErrorType = Err<E>::ErrorType;
    using VariantType = std::variant<Ok<T>, Err<E>>;

    /**
     * @brief Result iterator, will yield one value if the result is not
     * error, otherwise nothing.
     *
     */
    class Iterator {
      public:
        using difference_type = std::ptrdiff_t;
        using value_type = ValueType;

      private:
        Result *_result{nullptr};

      public:
        constexpr Iterator(Result &result)
            : _result(result.is_err() ? nullptr : &result) {}

        constexpr Iterator() = default;
        constexpr ~Iterator() = default;

        NEXUS_COPY_DEFAULT(Iterator);
        NEXUS_MOVE_DEFAULT(Iterator);

        /**
         * @brief Unwrap the value (to a reference, do not consuming the
         * result).
         *
         * @return ValueType& Result value.
         */
        constexpr auto operator*() const -> ValueType & {
            [[likely]] if (auto *value =
                               std::get_if<Ok<ValueType>>(&_result->_base());
                           value != nullptr) {
                return value->value();
            }

            throw Error(Error::Unwrap, "Result cannot be dereferenced");
        }

        /**
         * @brief Move iterator to the end.
         *
         * @return Iterator& End iterator.
         */
        constexpr auto operator++() -> Iterator & {
            _result = nullptr;
            return *this;
        }

        /**
         * @brief Move iterator to the end.
         *
         * @return Iterator Previous iterator.
         */
        constexpr auto operator++(int) -> Iterator {
            auto tmp = *this;
            ++*this;
            return tmp;
        }

        /**
         * @brief Check if iterator equals to other.
         *
         * @param other Another iterator.
         * @return true Iterators have the same result pointer.
         * @return false Iterators don't have the same result pointer.
         */
        constexpr auto operator==(const Iterator &other) const -> bool {
            return _result == other._result;
        }
    };

    /**
     * @brief Result error iterator, will yield one error if the result is
     * error, otherwise nothing.
     *
     */
    class ErrorIterator {
      public:
        using difference_type = std::ptrdiff_t;
        using value_type = ErrorType;

      private:
        Result *_result{nullptr};

      public:
        ErrorIterator(Result &result)
            : _result(result.is_ok() ? nullptr : &result) {}

        ErrorIterator() = default;
        ~ErrorIterator() = default;

        NEXUS_COPY_DEFAULT(ErrorIterator);
        NEXUS_MOVE_DEFAULT(ErrorIterator);

        /**
         * @brief Unwrap the error (to a reference, do not consuming the
         * result).
         *
         * @return ErrorIterator& Result error.
         */
        auto operator*() const -> ErrorType & {
            [[likely]] if (auto *value =
                               std::get_if<Err<ErrorType>>(&_result->_base());
                           value != nullptr) {
                return value->error();
            }

            throw Error(Error::Unwrap, "Result cannot be dereferenced");
        }

        /**
         * @brief Move iterator to the end.
         *
         * @return ErrorIterator& End iterator.
         */
        auto operator++() -> ErrorIterator & {
            _result = nullptr;
            return *this;
        }

        /**
         * @brief Move iterator to the end.
         *
         * @return ErrorIterator Previous iterator.
         */
        auto operator++(int) -> ErrorIterator {
            auto tmp = *this;
            ++*this;
            return tmp;
        }

        /**
         * @brief Check if iterator equals to other.
         *
         * @param other Another iterator.
         * @return true Iterators have the same result pointer.
         * @return false Iterators don't have the same result pointer.
         */
        auto operator==(const ErrorIterator &other) const -> bool {
            return _result == other._result;
        }
    };

    /**
     * @brief Enumerate helper for error type.
     *
     */
    class ErrorEnumerator {
      private:
        Result *_result;

      public:
        ErrorEnumerator(Result &result) : _result(&result) {}

        ~ErrorEnumerator() = default;

        NEXUS_COPY_DEFAULT(ErrorEnumerator);
        NEXUS_MOVE_DEFAULT(ErrorEnumerator);

        /**
         * @brief Get the begin iterator.
         *
         * @return ErrorIterator Begin iterator.
         */
        [[nodiscard]] NEXUS_INLINE auto begin() -> ErrorIterator {
            return _result->ebegin();
        }

        /**
         * @brief Get the end iterator.
         *
         * @return ErrorIterator End iterator.
         */
        [[nodiscard]] NEXUS_INLINE auto end() -> ErrorIterator {
            return _result->eend();
        }
    };

    template <typename... Args>
    constexpr Result(Args &&...args)
        : VariantType(std::forward<Args>(args)...) {}

    constexpr ~Result() = default;

    NEXUS_COPY_DEFAULT(Result);
    NEXUS_MOVE_DEFAULT(Result);

    /**
     * @brief Get the begin iterator.
     *
     * @return Iterator Begin iterator.
     */
    [[nodiscard]] constexpr NEXUS_INLINE auto begin() -> Iterator {
        return Iterator(*this);
    }

    /**
     * @brief Get the end iterator.
     *
     * @return Iterator End iterator.
     */
    [[nodiscard]] constexpr NEXUS_INLINE auto end() -> Iterator {
        return Iterator();
    }

    /**
     * @brief Get the begin error iterator.
     *
     * @return ErrorIterator Begin error iterator.
     */
    [[nodiscard]] constexpr NEXUS_INLINE auto ebegin() -> ErrorIterator {
        return ErrorIterator(*this);
    }

    /**
     * @brief Get the end error iterator.
     *
     * @return ErrorIterator End error iterator.
     */
    [[nodiscard]] constexpr NEXUS_INLINE auto eend() -> ErrorIterator {
        return ErrorIterator();
    }

    /**
     * @brief Get the error enumerator.
     *
     * @return ErrorEnumerator error enumerator.
     */
    [[nodiscard]] constexpr NEXUS_INLINE auto error_enumerator()
        -> ErrorEnumerator {
        return ErrorEnumerator(*this);
    }

    /**
     * @brief Return res if result is not error, otherwise return current
     * error.
     *
     * @tparam U Another value type.
     * @param res Another result.
     * @return Result<U, E> Final result.
     */
    template <typename U>
    [[nodiscard]] constexpr auto both(Result<U, E> &&res) -> Result<U, E> {
        if (is_err()) {
            return Err(unwrap_err());
        }
        return std::move(res);
    }

    /**
     * @brief Return conv result if result is not error, otherwise return
     * current error.
     *
     * @param conv Result converter.
     * @return Result<U, E> Final result.
     */
    [[nodiscard]] constexpr auto both_and(auto &&conv) -> decltype(auto)
        requires(IsResult<std::invoke_result_t<decltype(conv), ValueType>>)
    {
        using RealValueType =
            std::invoke_result_t<decltype(conv), ValueType>::ValueType;
        using FinalResultType = Result<RealValueType, E>;

        if (is_err()) {
            return FinalResultType(Err(unwrap_err()));
        }
        return conv(unwrap());
    }

    /**
     * @brief Return value if result is not error, otherwise return new result.
     *
     * @tparam F Another error type.
     * @param res Another result.
     * @return Result<T, F> Final result.
     */
    template <typename F>
    [[nodiscard]] constexpr auto either(Result<T, F> &&res) -> Result<T, F> {
        if (is_err()) {
            return std::move(res);
        }
        return Ok(unwrap());
    }

    /**
     * @brief Return result value if result is not error, otherwise return
     * converted result.
     *
     * @param conv Result converter.
     * @return decltype(auto) Final result.
     */
    [[nodiscard]] constexpr auto either_or(auto &&conv) -> decltype(auto)
        requires(IsResult<std::invoke_result_t<decltype(conv), ErrorType>>)
    {
        using RealErrorType =
            std::invoke_result_t<decltype(conv), ErrorType>::ErrorType;
        using FinalResultType = Result<T, RealErrorType>;

        if (is_err()) {
            return conv(unwrap_err());
        }
        return FinalResultType(Ok(unwrap()));
    }

    /**
     * @brief Convert Result<Result<T, E>, E> to Result<T, E>.
     *
     * @return decltype(auto) Final result.
     */
    [[nodiscard]] constexpr auto flattern() -> decltype(auto)
        requires(IsResult<ValueType> &&
                 std::is_same_v<typename ValueType::ErrorType, ErrorType>)
    {
        using FinalResultType = Result<typename ValueType::ValueType, E>;

        if (is_err()) {
            return FinalResultType(Err(unwrap_err()));
        }
        return unwrap();
    }

    /**
     * @brief Inspect value in result.
     *
     * @param func Inspect function.
     * @return Result Moved result.
     */
    [[nodiscard]] constexpr auto inspect(auto &&func) -> Result {
        for (const auto &value : *this) {
            func(value);
        }

        return std::move(*this);
    }

    /**
     * @brief Inspect error in result.
     *
     * @param func Inspect function.
     * @return Result Moved result.
     */
    [[nodiscard]] constexpr auto inspect_err(auto &&func) -> Result {
        for (const auto &err : this->error_enumerator()) {
            func(err);
        }

        return std::move(*this);
    }

    /**
     * @brief Check if result is error.
     *
     * @return true Result is error.
     * @return false Result is not error.
     */
    [[nodiscard]] constexpr NEXUS_INLINE auto is_err() -> bool {
        return std::get_if<Err<ErrorType>>(&_base()) != nullptr;
    }

    /**
     * @brief Check if result is error and matches predicate.
     *
     * @return true Result is error and matches predicate.
     * @return false Result is not error or does not match predicate.
     */
    [[nodiscard]] constexpr auto is_err_and(auto &&pred) -> bool {
        if (auto *err = std::get_if<Err<ErrorType>>(&_base()); err != nullptr) {
            return pred(std::move(err->error()));
        }
        return false;
    }

    /**
     * @brief Check if result is value.
     *
     * @return true Result is value.
     * @return false Result is not value.
     */
    [[nodiscard]] constexpr NEXUS_INLINE auto is_ok() -> bool {
        return std::get_if<Ok<ValueType>>(&_base()) != nullptr;
    }

    /**
     * @brief Check if result is value and matches predicate.
     *
     * @return true Result is value and matches predicate.
     * @return false Result is not value or does not match predicate.
     */
    [[nodiscard]] constexpr auto is_ok_and(auto &&pred) -> bool {
        if (auto *value = std::get_if<Ok<ValueType>>(&_base());
            value != nullptr) {
            return pred(std::move(value->value()));
        }
        return false;
    }

    /**
     * @brief Get and consume the result, throw if the result is an error.
     *
     * @param msg Error message when throw.
     * @return ValueType Result value.
     */
    [[nodiscard]] constexpr auto expect(std::string &&msg) -> ValueType {
        if (is_ok()) {
            return unwrap();
        }
        throw Error(Error::Unwrap, std::move(msg));
    }

    /**
     * @brief Get and consume the result, throw if the result is an error.
     *
     * @param msg Error message when throw.
     * @return ValueType Result value.
     */
    [[nodiscard]] constexpr NEXUS_INLINE auto expect(const std::string &msg)
        -> ValueType {
        expect(std::string(msg));
    }

    /**
     * @brief Get and consume the result, throw if the result is not an error.
     *
     * @param msg Error message when throw.
     * @return ErrorType Result error.
     */
    [[nodiscard]] constexpr auto expect_err(std::string &&msg) -> ErrorType {
        if (is_err()) {
            return unwrap_err();
        }
        throw Error(Error::Unwrap, std::move(msg));
    }

    /**
     * @brief Get and consume the result, throw if the result is not an error.
     *
     * @param msg Error message when throw.
     * @return ErrorType Result error.
     */
    [[nodiscard]] constexpr NEXUS_INLINE auto expect_err(const std::string &msg)
        -> ErrorType {
        return expect_err(std::string(msg));
    }

    /**
     * @brief Get and consume the result, throw if the result is an error.
     *
     * @return ValueType Result value.
     */
    [[nodiscard]] constexpr auto unwrap() -> ValueType {
        return std::visit(
            [](auto &&result) -> ValueType {
                using RealVariantType = std::decay_t<decltype(result)>;

                if constexpr (IsOk<RealVariantType>) {
                    return std::move(result.value());
                } else if constexpr (IsErr<RealVariantType>) {
                    throw Error(Error::Unwrap, "Result is an error ({})",
                                to_formattable(result.error()));
                } else {
                    static_assert(false,
                                  "Result has an unexpected variant type");
                }
            },
            _base());
    }

    /**
     * @brief Get and consume the result, or return the user-defined one if the
     * result is an error.
     *
     * @param value Fallback value.
     * @return ValueType Result value.
     */
    [[nodiscard]] constexpr auto unwrap_or(ValueType &&value) -> ValueType {
        return std::visit(
            [&](auto &&result) -> ValueType {
                using RealVariantType = std::decay_t<decltype(result)>;

                if constexpr (IsOk<RealVariantType>) {
                    return std::move(result.value());
                } else if constexpr (IsErr<RealVariantType>) {
                    return std::move(value);
                } else {
                    static_assert(false,
                                  "Result has an unexpected variant type");
                }
            },
            _base());
    }

    /**
     * @brief Get and consume the result, or return the user-defined one if the
     * result is an error.
     *
     * @param value Fallback value.
     * @return ValueType Result value.
     */
    [[nodiscard]] constexpr NEXUS_INLINE auto unwrap_or(const ValueType &value)
        -> ValueType {
        return unwrap_or(ValueType(value));
    }

    /**
     * @brief Get and consume the result, or return the default one (with
     * default constructor) if the result is an error.
     *
     * @return ValueType Result value.
     */
    [[nodiscard]] constexpr NEXUS_INLINE auto unwrap_or_default() -> ValueType {
        return unwrap_or(ValueType());
    }

    /**
     * @brief Get and consume the result, throw if the result is not an error.
     *
     * @return ErrorType Result error.
     */
    [[nodiscard]] constexpr auto unwrap_err() -> ErrorType {
        return std::visit(
            [](auto &&result) -> ErrorType {
                using RealVariantType = std::decay_t<decltype(result)>;

                if constexpr (IsErr<RealVariantType>) {
                    return std::move(result.error());
                } else if constexpr (IsOk<RealVariantType>) {
                    throw Error(Error::Unwrap, "Result is not an error ({})",
                                to_formattable(result.value()));
                } else {
                    static_assert(false,
                                  "Result has an unexpected variant type");
                }
            },
            _base());
    }

    /**
     * @brief Convert value if exists, return the new result.
     *
     * @param conv Convert function.
     * @return decltype(auto) New result.
     */
    [[nodiscard]] constexpr auto map(auto &&conv) -> decltype(auto) {
        using RealValueType = std::invoke_result_t<decltype(conv), ValueType>;
        using FinalResultType = Result<RealValueType, E>;

        if (is_ok()) {
            return FinalResultType(Ok(conv(unwrap())));
        }
        return FinalResultType(Err(unwrap_err()));
    }

    /**
     * @brief Convert error if exists, return the new result.
     *
     * @param conv Convert function.
     * @return decltype(auto) New result.
     */
    [[nodiscard]] constexpr auto map_err(auto &&conv) -> decltype(auto) {
        using RealErrorType = std::invoke_result_t<decltype(conv), ErrorType>;
        using FinalResultType = Result<T, RealErrorType>;

        if (is_err()) {
            return FinalResultType(Err(conv(unwrap_err())));
        }
        return FinalResultType(Ok(unwrap()));
    }

    /**
     * @brief Convert value if exists, or return the user-defined one.
     *
     * @tparam U New value type.
     * @param value User-defined value.
     * @param conv Convert function.
     * @return U Final value.
     */
    template <typename U>
    [[nodiscard]] constexpr auto map_or(U &&value, auto &&conv)
        -> decltype(auto)
        requires(
            std::same_as<std::invoke_result_t<decltype(conv), ValueType>, U>)
    {
        if (is_ok()) {
            return conv(unwrap());
        }
        return U(std::forward<U>(value));
    }

    /**
     * @brief Convert value if exists, or return the default one.
     *
     * @tparam U New value type.
     * @param conv Convert function.
     * @return U Final value.
     */
    template <typename U>
    [[nodiscard]] constexpr NEXUS_INLINE auto map_or_default(auto &&conv)
        -> decltype(auto) {
        return map_or(U(), std::forward<decltype(conv)>(conv));
    }

    /**
     * @brief Convert value or error to new value type.
     *
     * @param func Error convert function.
     * @param conv Value convert function.
     * @return decltype(auto) Convert function result.
     */
    [[nodiscard]] constexpr auto map_or_else(auto &&func, auto &&conv)
        -> decltype(auto)
        requires(std::same_as<std::invoke_result_t<decltype(conv), ValueType>,
                              std::invoke_result_t<decltype(func), ErrorType>>)
    {
        if (is_ok()) {
            return conv(unwrap());
        }
        return func(unwrap_err());
    }

  private:
    /**
     * @brief Get variant base from current result.
     *
     * @return VariantType& Variant base object.
     */
    [[nodiscard]] NEXUS_INLINE constexpr auto _base() -> VariantType & {
        return *static_cast<VariantType *>(this);
    }
};

} // namespace nexus
