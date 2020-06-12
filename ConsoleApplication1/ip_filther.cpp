#include "pch.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <queue>
#include <iterator>
#include <algorithm>

#include <boost/log/trivial.hpp>
#include <boost/optional.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/fusion/include/std_tuple.hpp>
#include <boost/fusion/algorithm/iteration/for_each.hpp>
#include <boost/fusion/algorithm/transformation/transform.hpp>
#include <boost/fusion/include/for_each.hpp>
#include <boost/fusion/include/transform.hpp>

namespace console
{
	void print_usage_and_exit()
	{
		BOOST_LOG_TRIVIAL(info) << "usage: -source=<filename> -out=<filename>";
		exit(0);
	}

	std::pair<boost::filesystem::path, boost::filesystem::path> get_args_and_exit_if_fail(int argc, char const *argv[])
	{
		if (argc <= 1)
			print_usage_and_exit();

		std::string const source_arg_prefix = "-in=",
			destination_arg_prefix = "-out=";

		boost::optional<boost::filesystem::path> ip_addresses_source_path, ip_addresses_destination_path;

		auto extract_arg = [](std::string const & arg_prefix, std::string &arg_as_str)
			-> boost::optional<std::string>
		{
			auto source_arg_begin = arg_as_str.find(arg_prefix);
			if (std::string::npos != source_arg_begin)
				return arg_as_str.substr(arg_prefix.size());
			return {};
		};
		for (auto begin = argv, end = argv + argc; begin != end; ++begin)
		{
			std::string arg_as_str(*begin);
			auto arg = extract_arg(source_arg_prefix, arg_as_str);

			if (!ip_addresses_source_path.is_initialized() && arg.is_initialized())
			{
				ip_addresses_source_path = arg.get();
				continue;
			}

			arg = extract_arg(destination_arg_prefix, arg_as_str);
			if(!ip_addresses_destination_path.is_initialized() && arg.is_initialized())
			{
				ip_addresses_destination_path = arg.get();
				continue;
			}
		}

		if (!ip_addresses_source_path || !ip_addresses_destination_path)
			print_usage_and_exit();
		return std::make_pair(
			std::move(ip_addresses_source_path.get()), 
			std::move(ip_addresses_destination_path.get())
		);
	}
}

namespace ip
{
	using repeats = size_t;

	using ip_v4_part = uint8_t;
	static char const *ip_v4_part_delimiter = ".";
	namespace
	{
		template<typename Part, typename SecondParts>
		using ip_pool_parts = std::map<
			Part,
			SecondParts,
			std::greater<Part>
		>;

		template<typename Part>
		using closing_ip_parts = std::map<
			Part,
			repeats,
			std::greater<Part>
		>;
	}

	using ip_v4 = std::tuple<ip_v4_part, ip_v4_part, ip_v4_part, ip_v4_part>;
	using ip_v4_pool = ip_pool_parts<
		ip_v4_part,
		ip_pool_parts<
			ip_v4_part,
			ip_pool_parts<
				ip_v4_part,
				closing_ip_parts<ip_v4_part>
			>
		>
	>;

	template<typename Ip>
	std::string to_string(Ip const &ip)
	{
		static_assert(false, "to_string is not implemented for this ip type.");
	}
	template<>
	std::string to_string<ip_v4>(ip_v4 const &ip)
	{
		return std::to_string(std::get<0>(ip))
			+ "." + std::to_string(std::get<1>(ip))
			+ "." + std::to_string(std::get<2>(ip))
			+ "." + std::to_string(std::get<3>(ip));
	}

	template<typename Iterator, typename Callback>
	void for_each(Iterator begin, Iterator end, Callback &&call_me)
	{
		std::for_each
		(
			begin,
			end,
			[callback = std::forward<Callback>(call_me)](decltype(*begin) ip_part1) mutable
			{
				for_each(
					ip_part1.first, 
					ip_part1.second.begin(), 
					ip_part1.second.end(), 
					callback
				);
			}
		);
	}

	template<typename Iterator, typename Callback>
	void for_each(ip_v4_part ip_first_part, Iterator begin, Iterator end, Callback &&call_me)
	{
		std::for_each
		(
			begin,
			end,
			[ip_first_part, callback = std::forward<Callback>(call_me)](decltype(*begin) ip_part2) mutable
			{
				for_each(
					std::make_tuple(ip_first_part, ip_part2.first),
					ip_part2.second.begin(),
					ip_part2.second.end(),
					callback
				);
			}
		);
	}

	template<typename Iterator, typename Callback>
	void for_each(std::tuple<ip_v4_part, ip_v4_part> ip, Iterator begin, Iterator end, Callback &&call_me)
	{
		std::for_each
		(
			begin,
			end,
			[&ip, callback = std::forward<Callback>(call_me)](decltype(*begin) ip_part3) mutable
			{
				for_each(
					std::make_tuple(std::get<0>(ip), std::get<1>(ip), ip_part3.first),
					ip_part3.second.begin(),
					ip_part3.second.end(),
					callback
				);
			}
		);
	}

	template<typename Iterator, typename Callback>
	void for_each(std::tuple<ip_v4_part, ip_v4_part, ip_v4_part> ip, Iterator begin, Iterator end, Callback &&call_me)
	{
		std::for_each
		(
			begin,
			end,
			[&ip, callback = std::forward<Callback>(call_me)](decltype(*begin) ip_part4) mutable
			{
				callback(
					std::make_tuple(std::get<0>(ip), std::get<1>(ip), std::get<2>(ip), ip_part4.first),
					ip_part4.second
				);
			}
		);
	}

	template<typename IpPool>
	IpPool extract_ip_pool(std::istream &source)
	{
		static_assert(false, "extract_ip_pool is not implemented for this ip pool type.");
	}

	template<>
	ip::ip_v4_pool extract_ip_pool<ip::ip_v4_pool>(std::istream &source)
	{
		ip::ip_v4_pool ip_v4_pool;

		size_t line_id{ 1 };
		std::string line;
		for (; std::getline(source, line); ++line_id)
		{
			std::vector<std::string> tokens;

			boost::split(tokens, line, boost::is_any_of("\t"));
			if (tokens.empty())
			{
				BOOST_LOG_TRIVIAL(warning) << "Empty parsing result"
					<< " at line №" << line_id;
				continue;
			}
			auto ip_v4_as_string = std::move(tokens.front());

			tokens.clear();
			boost::split(tokens, ip_v4_as_string, boost::is_any_of(ip::ip_v4_part_delimiter));

			decltype(auto) ip_parts_count = std::tuple_size<ip::ip_v4>::value;
			if (tokens.size() < ip_parts_count)
			{
				BOOST_LOG_TRIVIAL(warning) << "Ip parts count is " << tokens.size()
					<< ", when required " << ip_parts_count << line_id << "."
					<< " At line №" << line_id;
				continue;
			}
			tokens.resize(ip_parts_count);

			std::array<ip::ip_v4_part, ip_parts_count> ip_v4_parts;
			std::transform(tokens.begin(), tokens.end(), ip_v4_parts.begin(),
				[](decltype(*tokens.begin()) ip_part)
			{
				return boost::numeric_cast<ip::ip_v4_part>(boost::lexical_cast<size_t>(ip_part));
			}
			);

			++ip_v4_pool[ip_v4_parts[0]][ip_v4_parts[1]][ip_v4_parts[2]][ip_v4_parts[3]];
		}

		return std::move(ip_v4_pool);
	};
}

int main(int argc, char const *argv[])
{
	boost::filesystem::path ip_v4_pool_source_path, ip_v4_pool_destination_path;
	std::tie(ip_v4_pool_source_path, ip_v4_pool_destination_path) = console::get_args_and_exit_if_fail(argc, argv);

	std::fstream ip_v4_pool_source{ ip_v4_pool_source_path.string() };
	if(!ip_v4_pool_source.is_open())
	{
		BOOST_LOG_TRIVIAL(error) << "can't open source(in) file \"" << boost::filesystem::absolute(ip_v4_pool_source_path) << "\"";
		return -1;
	}

	auto ip_v4_pool = ip::extract_ip_pool<ip::ip_v4_pool>(ip_v4_pool_source);
	if (ip_v4_pool.empty())
	{
		BOOST_LOG_TRIVIAL(warning) << "here is no one ip address in file.";
		return 0;
	}

	std::ofstream ip_addresses_destination{ ip_v4_pool_destination_path.string() };
	if (!ip_v4_pool_source.is_open())
	{
		BOOST_LOG_TRIVIAL(error) << "can't open destination(out) file \"" << boost::filesystem::absolute(ip_v4_pool_destination_path) << "\"";
		return -1;
	}

	ip::for_each(
		ip_v4_pool.begin(),
		ip_v4_pool.end(),
		[&ip_addresses_destination](ip::ip_v4 ip, ip::repeats rep)
		{
			std::generate_n(
				std::ostream_iterator<decltype(ip::to_string(ip))>(ip_addresses_destination, "\n"),
				rep,
				[ip_as_string = ip::to_string(ip)]()
				{
					return ip_as_string;
				}
			);
		}
	);

	// filter by first byte and output
	// ip_v4 = filter(1)

	// 1.231.69.33
	// 1.87.203.225
	// 1.70.44.170
	// 1.29.168.152
	// 1.1.234.8
	ip::for_each(
		1,
		ip_v4_pool[1].begin(),
		ip_v4_pool[1].end(),
		[&ip_addresses_destination](ip::ip_v4 ip, ip::repeats rep)
		{
			std::generate_n(
				std::ostream_iterator<decltype(ip::to_string(ip))>(ip_addresses_destination, "\n"),
				rep,
				[ip_as_string = ip::to_string(ip)]()
				{
					return ip_as_string;
				}
			);
		}
	);

	// filter by first and second bytes and output
	// ip_v4 = filter(46, 70)

	// 46.70.225.39
	// 46.70.147.26
	// 46.70.113.73
	// 46.70.29.76
	ip::for_each(
		std::make_tuple(46, 70),
		ip_v4_pool[46][70].begin(),
		ip_v4_pool[46][70].end(),
		[&ip_addresses_destination](ip::ip_v4 ip, ip::repeats rep)
		{
			std::generate_n(
				std::ostream_iterator<decltype(ip::to_string(ip))>(ip_addresses_destination, "\n"),
				rep,
				[ip_as_string = ip::to_string(ip)]()
				{
					return ip_as_string;
				}
			);
		}
	);

	// filter by any byte and output
	// ip_v4 = filter_any(46)

	// 186.204.34.46
	// 186.46.222.194
	// 185.46.87.231
	// 185.46.86.132
	// 185.46.86.131
	// 185.46.86.131
	// 185.46.86.22
	// 185.46.85.204
	// 185.46.85.78
	// 68.46.218.208
	// 46.251.197.23
	// 46.223.254.56
	// 46.223.254.56
	// 46.182.19.219
	// 46.161.63.66
	// 46.161.61.51
	// 46.161.60.92
	// 46.161.60.35
	// 46.161.58.202
	// 46.161.56.241
	// 46.161.56.203
	// 46.161.56.174
	// 46.161.56.106
	// 46.161.56.106
	// 46.101.163.119
	// 46.101.127.145
	// 46.70.225.39
	// 46.70.147.26
	// 46.70.113.73
	// 46.70.29.76
	// 46.55.46.98
	// 46.49.43.85
	// 39.46.86.85
	// 5.189.203.46

	ip::for_each(
		ip_v4_pool.begin(),
		ip_v4_pool.end(),
		[&ip_addresses_destination](ip::ip_v4 ip, ip::repeats rep)
		{
			bool does_ip_fit_filter = false;
			boost::fusion::for_each(
				ip, 
				[accept_filther = boost::is_any_of(std::array<ip::ip_v4_part, 1>{46}), &does_ip_fit_filter]
				(ip::ip_v4_part &ip_part_value)
				{ 
					does_ip_fit_filter |= accept_filther(ip_part_value);
				}
			);
			if (!does_ip_fit_filter)
				return;

			std::generate_n(
				std::ostream_iterator<decltype(ip::to_string(ip))>(ip_addresses_destination, "\n"),
				rep,
				[ip_as_string = ip::to_string(ip)]()
				{
					return ip_as_string;
				}
			);
		}
	);
	return 0;
}
