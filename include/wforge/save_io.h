#ifndef WFORGE_SAVE_IO_H
#define WFORGE_SAVE_IO_H

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace wf {

class SaveKV {
public:
	SaveKV(const SaveKV &) = delete;
	SaveKV &operator=(const SaveKV &) = delete;

	static SaveKV &instance();

	void load();

	[[nodiscard]] std::optional<std::string> get(std::string_view key) const;

	[[nodiscard]] int getInt(std::string_view key, int default_val = 0) const;
	[[nodiscard]] bool getBool(
		std::string_view key, bool default_val = false
	) const;

	void set(std::string_view key, std::string_view value);
	void setInt(std::string_view key, int value);
	void setBool(std::string_view key, bool value);

	void remove(std::string_view key);

	void flush();

private:
	SaveKV() = default;

	std::unordered_map<std::string, std::string> _data;
	bool _dirty = false;
	int _flush_cooldown = 0;
};

} // namespace wf

#endif // WFORGE_SAVE_IO_H
