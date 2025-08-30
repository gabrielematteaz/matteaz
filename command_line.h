#ifndef MATTEAZ_COMMAND_LINE_H
#define MATTEAZ_COMMAND_LINE_H

#include <string_view>
#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <algorithm>
#include <string>

namespace matteaz
{
	struct arguments_sentinel
	{
		explicit arguments_sentinel() = default;
	};

	class arguments_iterator
	{
	public:
		using value_type = std::string_view;
		using difference_type = std::ptrdiff_t;
		using reference = const value_type &;
		using pointer = const value_type *;
		using iterator_category = std::forward_iterator_tag;

		arguments_iterator() = default;
		arguments_iterator(const arguments_iterator &) = default;
		arguments_iterator(arguments_iterator &&) = default;
		~arguments_iterator() = default;
		[[nodiscard]] bool operator == (const arguments_iterator &) const = default;
		arguments_iterator &operator = (const arguments_iterator &) = default;
		arguments_iterator &operator = (arguments_iterator &&) = default;

		constexpr arguments_iterator(std::string_view commandLine) :
			_CommandLine(commandLine),
			_Offset(0)
		{
			operator ++ ();
		}

		[[nodiscard]] constexpr bool operator == (const arguments_sentinel &) const noexcept
		{
			return _Offset == _CommandLine.npos;
		}

		constexpr arguments_iterator operator ++ (int)
		{
			auto previousThis = *this;

			operator ++ ();

			return previousThis;
		}

		[[nodiscard]] constexpr pointer operator -> () const noexcept
		{
			return &_Argument;
		}

		constexpr arguments_iterator &operator ++ ()
		{
			if (try_increment() == false)
				throw std::logic_error("unexpected end of argument");

			return *this;
		}

		[[nodiscard]] constexpr reference operator * () const noexcept
		{
			return _Argument;
		}

		[[nodiscard]] constexpr arguments_iterator begin() const noexcept
		{
			return *this;
		}

		[[nodiscard]] constexpr arguments_sentinel end() const noexcept
		{
			return arguments_sentinel();
		}

		[[nodiscard]] constexpr bool try_increment() noexcept
		{
			auto isDelimiter = [] (char character) constexpr noexcept
			{
				return character == ' ' || character == '\t' || character == '\n' || character == '\r' || character == '\f' || character == '\v';
			};

			auto first = std::ranges::find_if_not(_CommandLine.begin() + _Offset, _CommandLine.end(), isDelimiter);

			if (first == _CommandLine.end())
			{
				_Offset = _CommandLine.npos;

				return true;
			}

			auto current = first;
			bool skip = false;
			bool withinDoubleQuotes = false;
			bool withinSingleQuotes = false;

			for (; current != _CommandLine.end(); ++current)
			{
				if (skip)
					skip = false;
				else if (isDelimiter(*current) && withinDoubleQuotes == false && withinSingleQuotes == false)
					break;
				else if (*current == '"' && withinSingleQuotes == false)
					withinDoubleQuotes = !withinDoubleQuotes;
				else if (*current == '\\' && withinSingleQuotes == false)
					skip = true;
				else if (*current == '\'' && withinDoubleQuotes == false)
					withinSingleQuotes = !withinSingleQuotes;
			}

			if (withinDoubleQuotes || withinSingleQuotes)
				return false;

			_Offset = current - _CommandLine.begin();
			_Argument = _CommandLine.substr(first - _CommandLine.begin(), current - first);

			return true;
		}

		[[nodiscard]] constexpr std::string get_normalized_argument() const
		{
			std::string normalized;

			normalized.reserve(_Argument.length());

			auto first = _Argument.begin();
			auto current = first;
			bool skip = false;
			bool withinDoubleQuotes = false;
			bool withinSingleQuotes = false;
			bool update = false;

			for (; current != _Argument.end(); ++current)
			{
				if (skip)
				{
					if (withinDoubleQuotes == false || *current == '"' || *current == '\\')
					{
						normalized.append(first, current - 1);
						first = current;
					}

					skip = false;
				}
				else if (*current == '"' && withinSingleQuotes == false)
				{
					withinDoubleQuotes = !withinDoubleQuotes;
					update = true;
				}
				else if (*current == '\\' && withinSingleQuotes == false)
					skip = true;
				else if (*current == '\'' && withinDoubleQuotes == false)
				{
					withinSingleQuotes = !withinSingleQuotes;
					update = true;
				}

				if (update)
				{
					normalized.append(first, current);
					first = current + 1;
					update = false;
				}
			}

			normalized.append(first, current - skip);

			return normalized;
		}

	private:
		std::string_view _CommandLine;
		std::size_t _Offset = _CommandLine.npos;
		value_type _Argument;
	};

	struct options_sentinel
	{
		explicit options_sentinel() = default;
	};

	class options_iterator
	{
	public:
		using value_type = char;
		using difference_type = std::ptrdiff_t;
		using reference = const value_type &;
		using pointer = const value_type *;
		using iterator_category = std::forward_iterator_tag;

		options_iterator() = default;
		options_iterator(const options_iterator &) = default;
		options_iterator(options_iterator &&) = default;
		~options_iterator() = default;
		[[nodiscard]] bool operator == (const options_iterator &) const = default;
		options_iterator &operator = (const options_iterator &) = default;
		options_iterator &operator = (options_iterator &&) = default;

		constexpr options_iterator(const arguments_iterator &iterator, std::string_view options) :
			_Options(is_valid_options_string(options) ? options : throw std::invalid_argument("invalid options string")),
			_Iterator(iterator)
		{
			if (_Iterator != _Iterator.end())
			{
				_Normalized = _Iterator.get_normalized_argument();
				_Offset = 0;
				operator ++ ();
			}
		}

		[[nodiscard]] constexpr bool operator == (const options_sentinel &) const noexcept
		{
			return _Offset == _Normalized.npos;
		}

		constexpr options_iterator operator ++ (int)
		{
			auto previousThis = *this;

			operator ++ ();

			return previousThis;
		}

		[[nodiscard]] constexpr pointer operator -> () const noexcept
		{
			return &_Option;
		}

		constexpr options_iterator &operator ++ ()
		{
			auto iterator = _Iterator;
			std::optional < std::string > normalized;
			auto offset = _Offset;

			try
			{
				if (_Offset == _Normalized.length() && _Normalized.empty() == false)
				{
					++_Iterator;

					if (_Iterator == _Iterator.end())
					{
						_Offset = _Normalized.npos;

						return *this;
					}

					normalized = std::move(_Normalized);
					_Normalized = _Iterator.get_normalized_argument();
					_Offset = 0;
				}

				if (_Offset == 0)
				{
					if (_Normalized.starts_with('-') == false || _Normalized.length() == 1)
					{
						_Offset = _Normalized.npos;

						return *this;
					}

					if (_Normalized == "--")
					{
						++_Iterator;
						_Offset = _Normalized.npos;

						return *this;
					}

					_Offset = 1;
				}

				auto option = _Normalized[_Offset];

				switch (_Find(option))
				{
					case 2:
						++_Offset;

						if (_Offset == _Normalized.length())
						{
							++_Iterator;

							if (_Iterator == _Iterator.end())
								throw std::logic_error("missing option argument");

							if (normalized.has_value() == false)
								normalized = std::move(_Normalized);

							_Normalized = _Iterator.get_normalized_argument();
							_Argument = _Normalized;
						}
						else
							_Argument = std::string_view(_Normalized.begin() + _Offset, _Normalized.end());

						_Offset = _Normalized.length();
						_Option = option;
						_HasArgument = true;

						break;
					case 3:
						++_Offset;

						if (_Offset == _Normalized.length())
							_HasArgument = false;
						else
						{
							_Argument = std::string_view(_Normalized.begin() + _Offset, _Normalized.end());
							_HasArgument = true;
						}
					
						_Offset = _Normalized.length();
						_Option = option;

						break;
					default:
						++_Offset;
						_Argument = _Normalized;
						_Option = option;
						_HasArgument = true;

						break;
				}
			}
			catch (...)
			{
				_Iterator = iterator;

				if (normalized.has_value())
					_Normalized = std::move(*normalized);

				_Offset = offset;

				throw;
			}

			return *this;
		}

		[[nodiscard]] constexpr reference operator * () const noexcept
		{
			return _Option;
		}

		[[nodiscard]] constexpr options_iterator begin() const noexcept
		{
			return *this;
		}

		[[nodiscard]] constexpr options_sentinel end() const noexcept
		{
			return options_sentinel();
		}

		[[nodiscard]] constexpr const std::string_view *get_option_argument() const noexcept
		{
			return _HasArgument ? &_Argument : nullptr;
		}

		[[nodiscard]] constexpr arguments_iterator get_arguments_iterator() const noexcept
		{
			return _Iterator;
		}

		[[nodiscard]] static constexpr bool is_valid_options_string(std::string_view string) noexcept
		{
			bool start = false;

			for (auto current = string.begin(); current != string.end(); ++current)
			{
				if (start)
				{
					if (*current == '+' || *current == '?')
						start = false;
				}
				else
				{
					if (*current == '+' || *current == '?')
						return false;

					start = true;
				}
			}

			return true;
		}

	private:
		std::string_view _Options;
		arguments_iterator _Iterator;
		std::string _Normalized;
		std::string::size_type _Offset = _Normalized.npos;
		std::string_view _Argument;
		value_type _Option;
		bool _HasArgument;

		[[nodiscard]] constexpr int _Find(char option) const noexcept
		{
			for (auto current = _Options.begin(); current != _Options.end(); ++current)
			{
				if (*current == option)
				{
					auto next = current + 1;

					if (next == _Options.end())
						return 1;
					else if (*next == '+')
						return 2;
					else if (*next == '?')
						return 3;
					else
						return 1;
				}
			}

			return 0;
		}
	};
}

#endif