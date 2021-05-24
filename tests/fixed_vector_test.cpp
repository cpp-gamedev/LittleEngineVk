#include <kt/fixed_vector/fixed_vector.hpp>
#include <ktest/ktest.hpp>

namespace {
struct foo {
	static int total;
	static int alive;
	int inst = 0;
	int val = 0;

	foo(int val) : val(val) {
		inst = ++total;
		++alive;
	}
	foo(foo&& rhs) : val(rhs.val) {
		inst = ++total;
		++alive;
	}
	foo(foo const& rhs) : val(rhs.val) {
		inst = ++total;
		++alive;
	}
	~foo() { --alive; }
};

int foo::alive = 0;
int foo::total = 0;

TEST(fixed_vector_trivial) {
	kt::fixed_vector<int, 4> vec0;
	EXPECT_EQ(vec0.empty(), true);
	vec0 = decltype(vec0)(3, 5);
	EXPECT_EQ(vec0.capacity() - vec0.size(), 1U);
	vec0.clear();
	EXPECT_EQ(vec0.empty(), true);
	vec0 = {1, 2, 3, 4};
	EXPECT_EQ(vec0.front(), 1);
	EXPECT_EQ(vec0.back(), 4);
	vec0.pop_back();
	EXPECT_EQ(vec0.back(), 3);
	EXPECT_EQ(vec0[1], 2);
	auto vec1 = vec0;
	EXPECT_EQ(vec0.size(), vec1.size());
	{
		std::size_t idx = 0;
		for (int& i : vec0) { EXPECT_EQ(i, vec1.at(idx++)); }
	}
	{
		auto const& v = vec0;
		std::size_t idx = 0;
		for (int const& i : v) { EXPECT_EQ(i, vec1.at(idx++)); }
	}
	vec1 = std::move(vec0);
	EXPECT_EQ(vec0.empty(), true);
}

TEST(fixed_vector_class) {
	kt::fixed_vector<foo, 3> vec0;
	EXPECT_EQ(vec0.empty(), true);
	vec0 = decltype(vec0)(2, foo(2));
	EXPECT_EQ(vec0.capacity() - vec0.size(), 1U);
	EXPECT_EQ(foo::alive, 2);
	vec0.clear();
	EXPECT_EQ(foo::alive, 0);
	EXPECT_EQ(vec0.empty(), true);
	vec0 = {foo{5}};
	EXPECT_EQ(foo::alive, 1);
	EXPECT_EQ(&vec0.front(), &vec0.back());
	vec0.pop_back();
	EXPECT_EQ(vec0.empty(), true);
	EXPECT_EQ(foo::alive, 0);
	vec0 = {foo{1}, foo{2}};
	auto vec1 = vec0;
	EXPECT_EQ(vec0.size(), vec1.size());
	EXPECT_EQ(foo::alive, 4);
	{
		std::size_t idx = 0;
		for (foo& f : vec0) { EXPECT_EQ(f.val, vec1.at(idx++).val); }
	}
	{
		auto const& v = vec0;
		std::size_t idx = 0;
		for (foo const& f : v) { EXPECT_EQ(f.val, vec1.at(idx++).val); }
	}
	vec1 = std::move(vec0);
	EXPECT_EQ(vec0.empty(), true);
	EXPECT_EQ(foo::alive, 2);
	vec1.clear();
	EXPECT_EQ(foo::alive, 0);
}
} // namespace
