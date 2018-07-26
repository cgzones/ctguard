#include "sqlitestatement.hpp"

#include "sqliteexception.hpp"

#include <limits>
#include <sstream>

namespace ctguard::libs::sqlite {

sqlite_statement::sqlite_statement(const std::string & statement, sqlite_db & db) : m_statment{ nullptr, &::sqlite3_finalize }, m_db{ db }
{
    if (statement.size() >= std::numeric_limits<int>::max()) {
        throw sqlite_exception{ "Sql statement too long" };
    }

    sqlite3_stmt * tmp;

    if (sqlite3_prepare_v2(m_db.m_db.get(), statement.c_str(), static_cast<int>(statement.size()), &tmp, nullptr) != SQLITE_OK || tmp == nullptr) {
        throw sqlite_exception{ "Can not compile sql statement '" + statement + "': " + m_db.error_msg() };
    }

    m_statment = std::unique_ptr<sqlite3_stmt, decltype(&::sqlite3_finalize)>{ tmp, &::sqlite3_finalize };
}

void sqlite_statement::reset()
{
    if (sqlite3_reset(m_statment.get()) != SQLITE_OK) {
        throw sqlite_exception{ std::string("Can not reset sql statement: ") + m_db.error_msg() };
    }
}

void sqlite_statement::clear_bindings()
{
    if (sqlite3_clear_bindings(m_statment.get()) != SQLITE_OK) {
        throw sqlite_exception{ std::string("Can not clear sql statement bindings: ") + m_db.error_msg() };
    }
}

sqlite_statement::result sqlite_statement::run(bool query)
{
    const int state = sqlite3_step(m_statment.get());
    if (state == SQLITE_DONE) {
        return sqlite_statement::result{ *this, state };
    }

    if (query && state == SQLITE_ROW) {
        return sqlite_statement::result{ *this, state };
    }

    throw sqlite_exception{ "Can not run sql statement (" + std::to_string(state) + "): " + m_db.error_msg() };
}

void sqlite_statement::bind(int i, const std::string & value)
{
    if (value.size() >= std::numeric_limits<int>::max()) {
        throw sqlite_exception{ "Content string to bind too long" };
    }

    int ret;
    if ((ret = sqlite3_bind_text(m_statment.get(), i, value.c_str(), static_cast<int>(value.size()), SQLITE_STATIC)) != SQLITE_OK) {
        throw sqlite_exception{ "Can not bind string value to statement: " + std::to_string(ret) };
    }
}

void sqlite_statement::bind(int i, int value)
{
    int ret;
    if ((ret = sqlite3_bind_int(m_statment.get(), i, value)) != SQLITE_OK) {
        throw sqlite_exception{ "Can not bind int value to statement: " + std::to_string(ret) };
    }
}

void sqlite_statement::bind(int i, long value)
{
    int ret;
    if ((ret = sqlite3_bind_int64(m_statment.get(), i, value)) != SQLITE_OK) {
        throw sqlite_exception{ "Can not bind long value to statement: " };
    }
}

void sqlite_statement::bind(int i, sqlite3_int64 value)
{
    int ret;
    if ((ret = sqlite3_bind_int64(m_statment.get(), i, value)) != SQLITE_OK) {
        throw sqlite_exception{ "Can not bind sqlite3_int64  to statement: " + std::to_string(ret) };
    }
}

void sqlite_statement::bind(int i, unsigned value)
{
    if (value >= std::numeric_limits<int>::max()) {
        throw sqlite_exception{ "Content unsigned int to bind too long" };
    }

    int ret;
    if ((ret = sqlite3_bind_int(m_statment.get(), i, static_cast<int>(value))) != SQLITE_OK) {
        throw sqlite_exception{ "Can not bind unsigned value to statement: " + std::to_string(ret) };
    }
}

void sqlite_statement::bind(int i, unsigned long value)
{
    if (value >= std::numeric_limits<sqlite3_int64>::max()) {
        throw sqlite_exception{ "Content sqlite3_uint64 to bind too long" };
    }

    int ret;
    if ((ret = sqlite3_bind_int64(m_statment.get(), i, static_cast<sqlite3_int64>(value))) != SQLITE_OK) {
        throw sqlite_exception{ "Can not bind unsigned long value to statement: " + std::to_string(ret) };
    }
}

void sqlite_statement::bind(int i, sqlite3_uint64 value)
{
    if (value >= std::numeric_limits<sqlite3_int64>::max()) {
        throw sqlite_exception{ "Content sqlite3_uint64 to bind too long" };
    }

    int ret;
    if ((ret = sqlite3_bind_int64(m_statment.get(), i, static_cast<sqlite3_int64>(value))) != SQLITE_OK) {
        throw sqlite_exception{ "Can not bind sqlite3_uint64 value to statement: " + std::to_string(ret) };
    }
}

void sqlite_statement::bind(int i, const std::string_view & value)
{
    if (value.size() >= std::numeric_limits<int>::max()) {
        throw sqlite_exception{ "Content string_view to bind too long" };
    }

    int ret;
    if ((ret = sqlite3_bind_text(m_statment.get(), i, value.begin(), static_cast<int>(value.size()), SQLITE_STATIC)) != SQLITE_OK) {
        throw sqlite_exception{ "Can not bind string_view value to statement: " + std::to_string(ret) };
    }
}

}  // namespace ctguard::libs::sqlite
