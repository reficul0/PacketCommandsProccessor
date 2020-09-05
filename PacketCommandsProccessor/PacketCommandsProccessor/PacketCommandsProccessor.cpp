// InfinityMatrix.cpp : Этот файл содержит функцию "main". Здесь начинается и заканчивается выполнение программы.
//

#include "pch.h"

#include <cassert>
#include <iostream>
#include <locale>
#include <cctype>
#include <clocale>

#include <unordered_map>
#include <boost/optional.hpp>
#include <boost/signal.hpp>
#include <boost/bind.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/find.hpp>
#include <utility>

namespace tools
{
	template<typename ElementType>
	// Элемент не предназначен для корректной работы после уничтожения матрицы
	class Element
	{
		boost::optional<ElementType>& _element;
		boost::signal<void()> _on_value_added;
		boost::signal<void()> _on_value_deleted;

	public:
		Element(
			decltype(_element) element,
			std::function<void()> on_value_added,
			std::function<void()> on_value_deleted
		)
			: _element(element)
		{
			_on_value_added.connect(on_value_added);
			_on_value_deleted.connect(on_value_deleted);
		}
		Element(Element &&e) noexcept
			: _element(e._element)
			, _on_value_added(std::move(e._on_value_added))
			, _on_value_deleted(std::move(e._on_value_deleted))
		{
		}

		Element<ElementType>& operator=(ElementType const &val)
		{
			bool const were_empty = IsEmpty();
			_element = val;
			if (were_empty)
				_on_value_added();
			return *this;
		}
		Element<ElementType>& operator=(ElementType &&val)
		{
			bool const were_empty = IsEmpty();
			_element = val;
			if (were_empty)
				_on_value_added();
			return *this;
		}
		Element<ElementType>& operator=(boost::none_t none)
		{
			bool const were_not_empty = !IsEmpty();
			_element = none;
			if (were_not_empty)
				_on_value_deleted();
			return *this;
		}

		operator ElementType&()
		{
			assert(!IsEmpty());
			return *_element;
		}

		bool IsEmpty() const
		{
			return !(bool)_element;
		}
	};

	template<typename ElementType>
	class IMultidimensionalMatrix
	{
	public:
		virtual ~IMultidimensionalMatrix() = default;
		virtual Element<ElementType> operator[](size_t id) = 0;
		virtual IMultidimensionalMatrix<ElementType>& operator()(size_t id) = 0;
		virtual size_t size() const = 0;
	};


	template<typename ElementType>
	class MultidimensionalMatrix
		: public IMultidimensionalMatrix<ElementType>
	{
		using Dimensions = std::unordered_map<
			size_t/*index of other dimension or index of this dimension element*/,
			std::pair<
				boost::optional<ElementType>/*this dimension element value*/,
				std::unique_ptr<IMultidimensionalMatrix<ElementType>/*other dimension*/>
			>
		>;
		Dimensions _dimensions;
		size_t _count_of_values;

		boost::signal<void()> _on_value_added;
		boost::signal<void()> _on_value_deleted;
	public:
		using base_type = IMultidimensionalMatrix<ElementType>;
		MultidimensionalMatrix()
			: _count_of_values{ 0 }
		{
		}
		MultidimensionalMatrix(
			std::function<void()> on_value_added,
			std::function<void()> on_value_deleted
		)
			: _count_of_values{ 0 }
		{
			_on_value_added.connect(on_value_added);
			_on_value_deleted.connect(on_value_deleted);
		}

		// get this dimension element
		Element<ElementType> operator[](size_t id) override
		{
			return Element<ElementType>(
				_create_dim_if_required(id)->second.first,
				boost::bind(&MultidimensionalMatrix::OnValueAdded, this),
				boost::bind(&MultidimensionalMatrix::OnValueDeleted, this)
				);
		}
		// get dimension(create if not exists)
		IMultidimensionalMatrix<ElementType>& operator()(size_t id) override
		{
			auto &dim = _create_dim_if_required(id)->second.second;
			return *(dim
				? dim
				: dim = std::make_unique<MultidimensionalMatrix>(
					boost::bind(&MultidimensionalMatrix::OnValueAdded, this),
					boost::bind(&MultidimensionalMatrix::OnValueDeleted, this)
					)
				);
		}

		size_t size() const override
		{
			return _count_of_values;
		}
	private:
		typename Dimensions::iterator _create_dim_if_required(size_t id)
		{
			auto element = _dimensions.find(id);
			if (element == _dimensions.end())
			{
				bool is_success = false;
				typename Dimensions::iterator iterator;
				std::tie(iterator, is_success) = _dimensions.emplace(
					std::piecewise_construct,
					std::forward_as_tuple(id),
					std::forward_as_tuple(boost::none, nullptr)
				);

				if (!is_success)
					throw std::exception{};
				return iterator;
			}
			return element;
		}

		// виртуальность, чтобы если что прикрутить потокобезопасность
		virtual void OnValueAdded()
		{
			++_count_of_values;
			_on_value_added();
		}
		virtual void OnValueDeleted()
		{
			--_count_of_values;
			_on_value_deleted();
		}
	};
}

class Interpreter
{
public:
	using callback_type = std::function<void(std::string&)>;
	using open_closed_callback_type = std::function<void(std::string&, bool)>;
private:
	std::string data;
	std::list<tools::IMultidimensionalMatrix<callback_type>*> active_closing_subscribers;
	tools::MultidimensionalMatrix<callback_type> closing_subscribers;
	
	std::list< std::pair<char, std::pair<char, open_closed_callback_type>*>> active_open_closed_subscribers;
	std::unordered_map<char, std::pair<char, open_closed_callback_type>> open_closed_subscribers;
public:
	void Interpret(char c)
	{
		bool any_notified = false;
		{
			if (!closing_subscribers[c].IsEmpty())
				_SubscriberBeginningMeet(c, &closing_subscribers);
	
			if (!active_closing_subscribers.empty())
			{
				_NotifySubscribersAndEraseIfNoMoreInDimension(c);
				any_notified = true;
			}
			
			auto &subscribers_dim = closing_subscribers(c);
			if (subscribers_dim.size())
				_SubscriberBeginningMeet(c, &subscribers_dim);
		}

		{

			bool any_open_closed_notified = false;
			if (!open_closed_subscribers.empty())
				any_open_closed_notified = _NotifyOpenClosedSubscribersAndEraseIfNoMoreInDimension(c);
			
			auto subscriber = open_closed_subscribers.find(c);
			if (subscriber != open_closed_subscribers.end())
			{
				_OpenClosedSubscriberOpeningMeet(c, subscriber->first, &subscriber->second);
				return;
			}
			
			if (any_open_closed_notified)
				return;
		}

		if (any_notified || !active_closing_subscribers.empty())
			return;
		
		data += c;
	}

	void Subscribe(std::string call_when_meet_me, callback_type fun)
	{
		assert(!call_when_meet_me.empty());
		decltype(closing_subscribers)::base_type *current_dim = &closing_subscribers;
		for (size_t i = 0; i < call_when_meet_me.size() - 1; ++i)
			current_dim = &(*current_dim)(call_when_meet_me[i]);
		
		(*current_dim)[call_when_meet_me.back()] = std::move(fun);
	}
	void Subscribe(char open, char close, open_closed_callback_type fun)
	{
		open_closed_subscribers[open] = std::make_pair(close, std::move(fun));
	}
private:
	void _SubscriberBeginningMeet(char c, tools::IMultidimensionalMatrix<callback_type> *subscribers)
	{
		active_closing_subscribers.push_back(subscribers);
	}
	void _NotifySubscribersAndEraseIfNoMoreInDimension(char c)
	{
		{
			for (auto *subscribers : active_closing_subscribers)
			{
				auto subscriber_callback = (*subscribers)[c];
				if (!subscriber_callback.IsEmpty())
					((callback_type&)subscriber_callback)(data);
			}

			std::list<tools::IMultidimensionalMatrix<callback_type>*> to_remove;
			for (auto *subscribers : active_closing_subscribers)
			{
				auto &subscribers_dim = (*subscribers)(c);
				if (const auto any_subscriber = subscribers_dim.size())
					active_closing_subscribers.push_back(&subscribers_dim);
				else
					to_remove.push_back(subscribers);
			}
			for (auto *subscribers : to_remove)
					active_closing_subscribers.remove(subscribers);
		}
	}

	void _OpenClosedSubscriberOpeningMeet(char c, char opening, std::pair<char, open_closed_callback_type> *subscriber)
	{
		active_open_closed_subscribers.emplace_back(opening, subscriber);
		subscriber->second(data, true);
	}
	bool _NotifyOpenClosedSubscribersAndEraseIfNoMoreInDimension(char c)
	{
		std::unordered_map< std::pair<char, open_closed_callback_type>*, bool > notified;
		
		for (auto &subscribers : active_open_closed_subscribers)
			if (subscribers.second->first == c )
			{
				{
					auto already_notified_sub = notified.find(subscribers.second);
					if (already_notified_sub != notified.end())
						continue;
				}

				subscribers.second->second(data, false);
				notified[subscribers.second] = true;
			}

		for (auto &subscriber : notified)
		{
			auto sub = std::find_if(
				active_open_closed_subscribers.begin(), active_open_closed_subscribers.end(), 
				[&subscriber](std::pair<char, std::pair<char, open_closed_callback_type>*> &val)
				{
					return val.second == subscriber.first;
				}
			);

			active_open_closed_subscribers.erase(sub);
		}
		
		return !notified.empty();
	}
};

int main()
{
	size_t count_of_commands_in_sequence = 3;
	
	std::vector<std::string> commands_example ={
		"cmd1\r\n", "cmd2\r\n","cmd3\r\n", "cmd4\r\n", "cmd5\r\n", "\r\n"
	};

	std::vector<std::string> packed_commands;
	bool is_open_block_context = bool{ false };

	static auto write_packed_commands = [](std::vector<std::string>&commands)
	{
		std::cout << "bulk: ";
		std::copy(commands.begin(), commands.end(), std::ostream_iterator<std::string>(std::cout, ", "));
		std::cout << std::endl;
		commands.clear();
	};
	
	auto pack_command = [&packed_commands, count_of_commands_in_sequence, &is_open_block_context](std::string &data)
	{
		bool const is_closing_line = data.empty();
		if (!is_closing_line)
			packed_commands.push_back(std::move(data));
		
		if (packed_commands.size() && !is_open_block_context && (packed_commands.size() == count_of_commands_in_sequence || is_closing_line) )
			write_packed_commands(packed_commands);
	};
	
	Interpreter interpreter;
	interpreter.Subscribe("\r\n", pack_command);
	
	for (auto &command : commands_example)
		for (auto character : command)
			interpreter.Interpret(character);


	std::vector<std::string> commands_example2 = {
		"cmd1\r\n", "cmd2\r\n","cmd3\r\n", "{\r\n", "cmd4\r\n", "cmd5\r\n", "}\r\n", "cmd6\r\n", "\r\n"
	};

	auto braced_command = [&packed_commands, count_of_commands_in_sequence, &is_open_block_context, enclosure = size_t{ 0 }]
	(std::string &data, bool true_is_opening_false_is_closing) mutable
	{
		if (true_is_opening_false_is_closing)
		{
			++enclosure;
			is_open_block_context = true;
		}
		else
			--enclosure;
		if (!enclosure && !packed_commands.empty())
		{
			write_packed_commands(packed_commands);
			is_open_block_context = false;
		}
	};
	
	interpreter.Subscribe('{', '}', braced_command);
	
	for (auto &command : commands_example2)
		for (auto character : command)
			interpreter.Interpret(character);
	
	std::vector<std::string> commands_example3 = {
		"cmd1\r\n", "cmd2\r\n","cmd3\r\n", "{\r\n", "cmd4\r\n", "{\r\n", "cmd5\r\n", "}\r\n", "cmd6\r\n", "}\r\n", "cmd7\r\n", "\r\n"
	};

	for (auto &command : commands_example3)
		for (auto character : command)
			interpreter.Interpret(character);
}