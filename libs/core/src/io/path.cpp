#include <filesystem>
#include <sstream>
#include <core/io/path.hpp>
#include <core/os.hpp>

namespace le::io {
namespace stdfs = std::filesystem;

Path::Path(std::string_view str) {
	while (!str.empty()) {
		std::size_t count = 0;
		if (m_units.empty() && str[0] == '/') {
			++count;
		}
		while (count < str.size() && str[count] != '/' && str[count] != '\\') {
			++count;
		}
		if (count > 0) {
			m_units.push_back(std::string(str.data(), count));
			str = str.substr(count);
		} else if (!m_units.empty()) {
			str = str.substr(1);
		}
	}
	if (!m_units.empty()) {
		std::string ext;
		auto& name = m_units.back();
		if (auto search = name.find_last_of('.'); search > 0 && search < name.size()) {
			ext = name.substr(search);
			name = name.substr(0, search);
		}
		if (!ext.empty()) {
			m_units.push_back(std::move(ext));
		}
	}
}

bool Path::has_parent_path() const {
	return m_units.size() > 2 || (m_units.size() > 1 && m_units.back()[0] != '.');
}

bool Path::empty() const {
	return m_units.empty();
}

bool Path::has_filename() const {
	return !m_units.empty() && (m_units.size() > 2 || m_units.back()[0] != '.');
}

bool Path::has_extension() const {
	return !m_units.empty() && m_units.back()[0] == '.';
}

bool Path::has_root_directory() const {
	return !m_units.empty() && (m_units.front().front() == '/' || m_units.front().back() == ':');
}

Path Path::parent_path() const {
	auto parent = m_units;
	if (!parent.empty() && parent.back()[0] == '.') {
		parent.pop_back();
	}
	if (!parent.empty()) {
		parent.pop_back();
	}
	return Path(std::move(parent));
}

Path Path::filename() const {
	if (!m_units.empty()) {
		if (m_units.size() == 1 && m_units.back()[0] == '/') {
			return Path(m_units.back().substr(1));
		}
		if (m_units.back()[0] == '.') {
			return m_units.size() > 1 ? Path(m_units[m_units.size() - 2]) : Path(m_units.back());
		}
		return Path(m_units.back());
	}
	return Path();
}

Path Path::extension() const {
	return has_extension() ? Path(m_units.back()) : Path();
}

std::string Path::string() const {
	return to_string(g_separator);
}

std::string Path::generic_string() const {
	return to_string('/');
}

void Path::clear() {
	m_units.clear();
}

Path& Path::append(Path const& rhs) {
	auto copy = rhs.m_units;
	if (!copy.empty()) {
		m_units.reserve(m_units.size() + copy.size());
		for (auto& item : copy) {
			while (item[0] == '/' || item[0] == '\\') {
				item = item.substr(1);
			}
			m_units.push_back(std::move(item));
		}
	}
	return *this;
}

Path& Path::operator/=(Path const& rhs) {
	return append(rhs);
}

Path& Path::concat(Path const& rhs) {
	if (m_units.empty()) {
		m_units = rhs.m_units;
		return *this;
	}
	if (rhs.empty()) {
		return *this;
	}
	if (m_units.size() > 1 && m_units.back()[0] == '.') {
		m_units[m_units.size() - 2] += std::move(m_units.back());
		m_units.pop_back();
	}
	std::string ext;
	auto units = rhs.m_units;
	if (units.back()[0] == '.') {
		ext = std::move(units.back());
	}
	auto squash = units.front();
	while (!squash.empty() && squash[0] == '/') {
		squash = squash.substr(1);
	}
	m_units.back() += std::move(squash);
	std::size_t const end = ext.empty() ? units.size() : units.size() - 1;
	for (std::size_t idx = 1; idx < end; ++idx) {
		std::string const& unit = units[idx];
		if (unit[0] == '.') {
			m_units.back() += unit;
		} else {
			m_units.push_back(unit);
		}
	}
	if (!ext.empty()) {
		m_units.push_back(std::move(ext));
	}
	return *this;
}

Path& Path::operator+=(Path const& rhs) {
	return concat(rhs);
}

Path::Path(std::vector<std::string>&& units) : m_units(std::move(units)) {
}

std::string Path::to_string(char separator) const {
	std::stringstream str;
	if (!m_units.empty()) {
		auto copy = m_units;
		std::string ext;
		if (copy.back()[0] == '.') {
			ext = std::move(copy.back());
			copy.pop_back();
		}
		std::string name;
		if (!copy.empty()) {
			name = std::move(copy.back());
			copy.pop_back();
		}
		for (auto& item : copy) {
			str << item << separator;
		}
		str << name << ext;
		return str.str();
	}
	return {};
}

Path absolute(Path const& path) {
#if defined(LEVK_OS_ANDROID)
	return path;
#else
	return Path(stdfs::absolute(path.generic_string()).generic_string());
#endif
}

Path current_path() {
#if defined(LEVK_OS_ANDROID)
	return Path();
#else
	return Path(stdfs::current_path().generic_string());
#endif
}

bool is_regular_file(Path const& path) {
#if defined(LEVK_OS_ANDROID)
	return !path.empty();
#else
	return stdfs::is_regular_file(path.generic_string());
#endif
}

bool is_directory(Path const& path) {
#if defined(LEVK_OS_ANDROID)
	return !path.empty();
#else
	return stdfs::is_directory(path.generic_string());
#endif
}

bool operator==(Path const& lhs, Path const& rhs) {
	return lhs.generic_string() == rhs.generic_string();
}
bool operator!=(Path const& lhs, Path const& rhs) {
	return !(lhs == rhs);
}
} // namespace le::io
