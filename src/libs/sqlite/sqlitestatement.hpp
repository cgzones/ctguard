#pragma once

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "sqlitedb.hpp"
#include "sqliteexception.hpp"

namespace ctguard::libs::sqlite {

template<typename T>
struct to_int
{
    using type = int;
};

class sqlite_statement
{
    using state_t = std::result_of<decltype (&::sqlite3_step)(::sqlite3_stmt *)>::type;

    class row
    {
      public:
        // cppcheck-suppress constParameter
        explicit row(sqlite_statement & stmt) noexcept : m_stmt{ stmt } {}

        template<typename T>
        [[nodiscard]] T get(int i) const
        {
            return get(i, T());
        }

        template<typename... T>
        [[nodiscard]] std::tuple<T...> get_columns(typename to_int<T>::type... idxs) const
        {
            return std::make_tuple(get(idxs, T())...);
        }

        [[nodiscard]] const char * get(int i, const char *) const { return reinterpret_cast<const char *>(::sqlite3_column_text(m_stmt.m_statment.get(), i)); }

        [[nodiscard]] std::string_view get(int i, std::string_view) const
        {
            const char * content = reinterpret_cast<const char *>(::sqlite3_column_text(m_stmt.m_statment.get(), i));
            const int bytes{ ::sqlite3_column_bytes(m_stmt.m_statment.get(), i) };
            if (bytes < 0) {
                throw sqlite_exception{ "Can not get string_view: got negative value '" + std::to_string(bytes) + "'" };
            }
            return std::string_view{ content, static_cast<size_t>(bytes) };
        }

        [[nodiscard]] int get(int i, int) const { return ::sqlite3_column_int(m_stmt.m_statment.get(), i); }

        [[nodiscard]] unsigned int get(int i, unsigned int) const
        {
            const int r{ ::sqlite3_column_int(m_stmt.m_statment.get(), i) };
            if (r < 0) {
                throw sqlite_exception{ "Can not get unsigned int: got negative value '" + std::to_string(r) + "'" };
            }
            return static_cast<unsigned int>(r);
        }

        [[nodiscard]] unsigned long get(int i, unsigned long) const
        {
            const ::sqlite3_int64 r{ ::sqlite3_column_int64(m_stmt.m_statment.get(), i) };
            if (r < 0) {
                throw sqlite_exception{ "Can not get unsigned long: got negative value '" + std::to_string(r) + "'" };
            }
            return static_cast<unsigned long>(r);
        }

        [[nodiscard]] long get(int i, long) const { return ::sqlite3_column_int64(m_stmt.m_statment.get(), i); }

        [[nodiscard]] ::sqlite3_int64 get(int i, ::sqlite3_int64) const { return ::sqlite3_column_int64(m_stmt.m_statment.get(), i); }

      private:
        sqlite_statement & m_stmt;
    };

    class iterator
    {
      public:
        iterator(sqlite_statement & stmt, state_t state) noexcept : m_stmt{ stmt }, m_state{ state } {}

        [[nodiscard]] bool operator==(const iterator & other) const noexcept { return (m_stmt == other.m_stmt && m_state == other.m_state); }
        [[nodiscard]] bool operator!=(const iterator & other) const noexcept { return !(*this == other); }

        iterator & operator++() noexcept
        {
            m_state = ::sqlite3_step(m_stmt.m_statment.get());
            return *this;
        }

        [[nodiscard]] row operator*() const noexcept { return row{ m_stmt }; }

      private:
        sqlite_statement & m_stmt;
        state_t m_state;
    };

    class result
    {
      public:
        result(sqlite_statement & stmt, state_t state) noexcept : m_stmt{ stmt }, m_state{ state } {}

        [[nodiscard]] iterator begin() const noexcept { return iterator{ m_stmt, m_state }; }
        [[nodiscard]] iterator end() const noexcept { return iterator{ m_stmt, SQLITE_DONE }; }

        [[nodiscard]] auto size() const noexcept { return ::sqlite3_column_count(m_stmt.m_statment.get()); }
        [[nodiscard]] auto data_count() const noexcept { return ::sqlite3_data_count(m_stmt.m_statment.get()); }

      private:
        sqlite_statement & m_stmt;
        state_t m_state;
    };

  public:
    sqlite_statement(const std::string & statement, sqlite_db & db);

    result run(bool query = false);
    void reset();
    void clear_bindings();

    void bind(int i, const std::string & value);
    void bind(int i, int value);
    void bind(int i, long value);
    void bind(int i, ::sqlite3_int64 value);
    void bind(int i, unsigned value);
    void bind(int i, unsigned long value);
    void bind(int i, ::sqlite3_uint64 value);
    void bind(int i, const std::string_view & value);

    [[nodiscard]] bool operator==(const sqlite_statement & other) const noexcept { return (m_statment == other.m_statment && m_db == other.m_db); }

  private:
    std::unique_ptr<::sqlite3_stmt, decltype(&::sqlite3_finalize)> m_statment;
    sqlite_db & m_db;
};

} /* namespace ctguard::libs::sqlite */
