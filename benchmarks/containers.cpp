#include "nonius.hpp"

#include <boost/intrusive/set_hook.hpp>
#include <boost/intrusive/set.hpp>

#include <deque>
#include <vector>
#include <map>
#include <unordered_map>

using namespace boost::intrusive;

class my_item : public set_base_hook<> {
public:
	explicit my_item(int i) noexcept : _i(i) {}

private:
	friend struct my_item_key;
	
	int _i = 0;
	char data[100];
};

struct my_item_key {
	typedef int type;

	const type& operator()(const my_item& v) const {
		return v._i;
	}
};

using intrusive_set =
	set<
		my_item,
		key_of_value<my_item_key>
	>;

NONIUS_BENCHMARK("std::map", [](nonius::chronometer meter) {
	std::map<int, my_item> map;
	meter.measure([&](int i) {
		map.emplace(i, my_item{i});
	});
});

NONIUS_BENCHMARK("std::unordered_map", [](nonius::chronometer meter) {
	std::unordered_map<int, my_item> map;
	meter.measure([&](int i) {
		map.emplace(i, my_item{i});
	});
});

NONIUS_BENCHMARK("boost::intrusive::set over std::deque", [](nonius::chronometer meter) {
	std::deque<my_item> coll;
	intrusive_set set;
	meter.measure([&](int i) {
		coll.emplace_back(i);
		set.insert(coll.back());
	});
});
