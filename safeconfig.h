#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

namespace safeconfig
{
  inline namespace valuetypes
  {
    enum class ValueId
    {
      integer,
      string,
      real,
      unknown
    };

    using RealType = double;
    using IntegerType = int;
    using StringType = std::string;
    class UnknownType; // undefined

    template<ValueId valueId>
    using ValueTypeFromValueId =
      std::conditional_t<valueId == ValueId::real, RealType,
      std::conditional_t<valueId == ValueId::integer, IntegerType,
      std::conditional_t<valueId == ValueId::string, StringType,
      UnknownType>>>;

    template<class ValueType>
    static constexpr ValueId valueIdFromValueType =
      std::is_same_v<ValueType, RealType> ? ValueId::real :
      std::is_same_v<ValueType, IntegerType> ? ValueId::integer :
      std::is_same_v<ValueType, StringType> ? ValueId::string :
      ValueId::unknown;

    inline std::string valueNameFromValueId(ValueId id)
    {
      std::string name;

      switch (id)
      {
      case ValueId::real:
        name = "double";
        break;
      case ValueId::integer:
        name = "int";
        break;
      case ValueId::string:
        name = "std::string";
        break;
      case ValueId::unknown:
        name = "UnknownType";
        break;
      default:
        throw std::runtime_error("Not implemented");
      }

      return name;
    }
  }

  class ValueModel
  {
  public:
    ValueModel() = default;
    virtual ~ValueModel() = default;

    template<class TValueType>
    const TValueType& getValue() const;

    template<class TValueType>
    void setValue(const TValueType& value);

    virtual ValueId getValueId() const noexcept = 0;

    virtual bool operator==(const ValueModel& other) const noexcept = 0;

  private:
    std::string errMsgForBadGetValue(ValueId badValueId) const
    {
      return "Invalid usage of `getValue<" + valueNameFromValueId(badValueId) + ">()`;"
        " did you mean `getValue<" + valueNameFromValueId(getValueId()) + ">()`?";
    }

    std::string errMsgForBadSetValue(ValueId badValueId) const
    {
      return "Invalid usage of `setValue(const " + valueNameFromValueId(badValueId) + "&)`;"
        " did you mean `setValue(const " + valueNameFromValueId(getValueId()) + "&)`?";
    }
  };

  template<ValueId id>
  class ConcreteValueModel : public ValueModel
  {
  public:
    static constexpr ValueId valueId = id;
    using ValueType = ValueTypeFromValueId<valueId>;

    static_assert(valueId != valuetypes::ValueId::unknown);
    static_assert(!std::is_same_v<ValueType, valuetypes::UnknownType>);

    ConcreteValueModel()
      : _value{}
    {}

    ConcreteValueModel(ValueType value)
      : _value(std::move(value))
    {}

    const ValueType& getValue() const
    {
      return _value;
    }

    void setValue(ValueType value)
    {
      _value = std::move(value);
    }

    virtual ValueId getValueId() const noexcept override
    {
      return valueId;
    }

    virtual bool operator==(const ValueModel& other) const noexcept override
    {
      return typeid(*this) == typeid(other) && _value == static_cast<const ConcreteValueModel&>(other)._value;
    }

  private:
    ValueType _value;
  };

  inline namespace valuemodels
  {
    using RealValueModel = ConcreteValueModel<ValueId::real>;
    using IntegerValueModel = ConcreteValueModel<ValueId::integer>;
    using StringValueModel = ConcreteValueModel<ValueId::string>;
    class UnknownValueModel; // undefined

    template<ValueId valueId>
    using ValueModelTypeFromValueId =
      std::conditional_t<valueId == RealValueModel::valueId, RealValueModel,
      std::conditional_t<valueId == IntegerValueModel::valueId, IntegerValueModel,
      std::conditional_t<valueId == StringValueModel::valueId, StringValueModel,
      UnknownValueModel>>>;

    inline std::unique_ptr<ValueModel> valueModelFromValueId(ValueId id)
    {
      std::unique_ptr<ValueModel> model;

      switch (id)
      {
      case ValueId::real:
        model = std::make_unique<RealValueModel>();
        break;
      case ValueId::integer:
        model = std::make_unique<IntegerValueModel>();
        break;
      case ValueId::string:
        model = std::make_unique<StringValueModel>();
        break;
      default:
        throw std::runtime_error("Not implemented");
      }

      return model;
    }
  }

  template<class TValueType>
  const TValueType& ValueModel::getValue() const
  {
    constexpr auto valueId = valueIdFromValueType<TValueType>;
    static_assert(valueId != ValueId::unknown);

    if (valueId != getValueId())
      throw std::runtime_error(errMsgForBadGetValue(valueId));

    return static_cast<const ValueModelTypeFromValueId<valueId>*>(this)->getValue();
  }

  template<class TValueType>
  void ValueModel::setValue(const TValueType& value)
  {
    constexpr auto valueId = valueIdFromValueType<TValueType>;
    static_assert(valueId != ValueId::unknown);

    if (valueId != getValueId())
      throw std::runtime_error(errMsgForBadSetValue(valueId));

    static_cast<ValueModelTypeFromValueId<valueId>*>(this)->setValue(value);
  }

  inline namespace constraints
  {
    enum class ConstraintId
    {
      numeric,
      choice,
      unknown
    };

    inline std::string constraintNameFromId(ConstraintId id)
    {
      std::string name;

      switch (id)
      {
      case ConstraintId::numeric:
        name = "NumericConstraint";
        break;
      case ConstraintId::choice:
        name = "ChoiceConstraint";
        break;
      default:
        throw std::runtime_error("Not implemented");
      }

      return name;
    }

    class Constraint
    {
    public:
      virtual ~Constraint() = default;
      virtual ValueId getValueId() const noexcept = 0;
      virtual ConstraintId getConstraintId() const noexcept = 0;
      virtual bool isValid(const ValueModel&) const noexcept = 0;
    };

    class NumericConstraint : public Constraint
    {
      using IntegerType = valuetypes::IntegerType;
      using RealType = valuetypes::RealType;

    public:
      NumericConstraint(IntegerType lowerBound, IntegerType upperBound)
      {
        setBounds(lowerBound, upperBound);
      }

      NumericConstraint(RealType lowerBound, RealType upperBound)
      {
        setBounds(lowerBound, upperBound);
      }

      void setBounds(IntegerType lowerBound, IntegerType upperBound)
      {
        setBoundsImpl(lowerBound, upperBound);
      }

      void setBounds(RealType lowerBound, RealType upperBound)
      {
        setBoundsImpl(lowerBound, upperBound);
      }

      virtual ValueId getValueId() const noexcept override
      {
        return _lb->getValueId();
      }

      virtual ConstraintId getConstraintId() const noexcept override
      {
        return ConstraintId::numeric;
      }

      virtual bool isValid(const ValueModel& valueModel) const noexcept override
      {
        bool valid = false;

        const ValueId modelValueId = valueModel.getValueId();

        if (modelValueId == this->getValueId())
        {
          switch (modelValueId)
          {
          case valueIdFromValueType<IntegerType>:
          {
            auto value = valueModel.getValue<IntegerType>();
            auto lower = _lb->getValue<IntegerType>();
            auto upper = _ub->getValue<IntegerType>();
            valid = value >= lower && value <= upper;
            break;
          }
          case valueIdFromValueType<RealType>:
          {
            auto value = valueModel.getValue<RealType>();
            auto lower = _lb->getValue<RealType>();
            auto upper = _ub->getValue<RealType>();
            valid = value >= lower && value <= upper;
            break;
          }
          default:
          {
            // Pass
          }
          }
        }

        return valid;
      }

    private:
      template<class T>
      void setBoundsImpl(T lowerBound, T upperBound)
      {
        if (lowerBound > upperBound)
          throw std::runtime_error("Parameter `lowerBound` cannot be greater than `upperBound`");

        constexpr ValueId effectiveValueId = valueIdFromValueType<T>;
        static_assert(effectiveValueId != ValueId::unknown);

        using EffectiveValueModel = ValueModelTypeFromValueId<effectiveValueId>;
        static_assert(!std::is_same_v<EffectiveValueModel, valuemodels::UnknownValueModel>);

        _lb = std::make_unique<EffectiveValueModel>(lowerBound);
        _ub = std::make_unique<EffectiveValueModel>(upperBound);
      }

      std::unique_ptr<ValueModel> _lb;
      std::unique_ptr<ValueModel> _ub;
    };

    class ChoiceConstraint : public Constraint
    {
    public:
      ChoiceConstraint(const std::vector<int>& validChoices)
        : _validChoices{}
      {
        setValidChoicesImpl(validChoices);
      }

      ChoiceConstraint(const std::vector<std::string>& validChoices)
        : _validChoices{}
      {
        setValidChoicesImpl(validChoices);
      }

      ChoiceConstraint(ChoiceConstraint&&) = default;
      ChoiceConstraint& operator=(ChoiceConstraint&&) = default;

      void setValidChoices(const std::vector<int>& choices)
      {
        setValidChoicesImpl(choices);
      }

      void setValidChoices(const std::vector<std::string>& choices)
      {
        setValidChoicesImpl(choices);
      }

      virtual ValueId getValueId() const noexcept override
      {
        return _validChoices.front()->getValueId();
      }

      virtual ConstraintId getConstraintId() const noexcept override
      {
        return ConstraintId::choice;
      }

      virtual bool isValid(const ValueModel& valueModel) const noexcept override
      {
        auto pos = std::find_if(_validChoices.cbegin(), _validChoices.cend(), [&valueModel](auto& validChoicePtr)
          { return valueModel == *validChoicePtr; }
        );

        return pos != _validChoices.cend();
      }

    private:
      template<class T>
      void setValidChoicesImpl(const std::vector<T>& choices)
      {
        constexpr ValueId effectiveValueId = valueIdFromValueType<T>;
        static_assert(effectiveValueId != ValueId::unknown);

        using EffectiveValueModel = ValueModelTypeFromValueId<effectiveValueId>;
        static_assert(!std::is_same_v<EffectiveValueModel, valuemodels::UnknownValueModel>);

        if (choices.empty())
          throw std::runtime_error("Parameter `choices` cannot be an empty vector");

        _validChoices.clear();
        _validChoices.reserve(choices.size());

        for (const auto& choice : choices)
          _validChoices.push_back(std::make_unique<EffectiveValueModel>(choice));
      }

    protected:
      std::vector<std::unique_ptr<ValueModel>> _validChoices;
    };
  }

  class Property
  {
  public:
    Property(std::string name, std::unique_ptr<Constraint> constraint)
      : _name(std::move(name))
      , _constraint(std::move(constraint))
      , _valueModel(valueModelFromValueId(_constraint->getValueId()))
    {
    }

    virtual ~Property() = default;

    Property(Property&&) = default;
    Property& operator=(Property&&) = default;

    const std::string& getName() const noexcept
    {
      return _name;
    }

    template<class T>
    const T& getValue() const
    {
      assertValuePassesConstraint();
      return _valueModel->getValue<T>();
    }

    template<class T>
    void setValue(const T& value)
    {
      _valueModel->setValue(value);
      assertValuePassesConstraint();
    }

    void setConstraint(std::unique_ptr<Constraint> constraint)
    {
      if (_constraint->getConstraintId() != constraint->getConstraintId())
      {
        throw std::runtime_error("Cannot set a new constraint for property named \"" + _name
          + "\"; attempted to replace a constraint of type `" + constraintNameFromId(_constraint->getConstraintId())
          + "` with an unrelated constraint of type `" + constraintNameFromId(constraint->getConstraintId()) + '`');
      }

      if (_constraint->getValueId() != constraint->getValueId())
      {
#if 1
        _valueModel = valueModelFromValueId(constraint->getValueId());
#else
        throw std::runtime_error("Cannot set a new constraint for property named \"" + _name
          + "\"; attempted to replace a constraint with a value type `" + valueNameFromValueId(_constraint->getValueId())
          + "` with a constraint with a value type `" + valueNameFromValueId(constraint->getValueId()) + '`');
#endif
      }

      _constraint = std::move(constraint);
    }

    const Constraint& getConstraint() const noexcept
    {
      return *_constraint;
    }

  private:
    void assertValuePassesConstraint() const
    {
      if (!_constraint->isValid(*_valueModel))
        throw std::runtime_error("Value of property named \"" + _name + "\" is invalid");
    }

    std::string _name;
    std::unique_ptr<Constraint> _constraint;
    std::unique_ptr<ValueModel> _valueModel;
  };

  inline namespace properties
  {
    class NumericProperty : public Property
    {
    public:
      using ConstraintType = NumericConstraint;

      NumericProperty(std::string name, std::pair<int, int> limits)
        : Property(std::move(name), std::make_unique<ConstraintType>(limits.first, limits.second))
      {}

      NumericProperty(std::string name, std::pair<double, double> limits)
        : Property(std::move(name), std::make_unique<ConstraintType>(limits.first, limits.second))
      {}

      // Throws unless the matching constructor was used
      void setConstraint(int lowerBound, int upperBound)
      {
        Property::setConstraint(std::make_unique<ConstraintType>(lowerBound, upperBound));
      }

      // Throws unless the matching constructor was used
      void setConstraint(double lowerBound, double upperBound)
      {
        Property::setConstraint(std::make_unique<ConstraintType>(lowerBound, upperBound));
      }
    };

    class ChoiceProperty : public Property
    {
    public:
      using ConstraintType = ChoiceConstraint;

      ChoiceProperty(std::string name, const std::vector<int>& choices)
        : Property(std::move(name), std::make_unique<ConstraintType>(choices))
      {}

      ChoiceProperty(std::string name, const std::vector<std::string>& choices)
        : Property(std::move(name), std::make_unique<ConstraintType>(choices))
      {}

      // Throws unless the matching constructor was used
      void setConstraint(const std::vector<int>& choices)
      {
        Property::setConstraint(std::make_unique<ConstraintType>(choices));
      }

      // Throws unless the matching constructor was used
      void setConstraint(const std::vector<std::string>& choices)
      {
        Property::setConstraint(std::make_unique<ConstraintType>(choices));
      }
    };
  }

  class JsonProxy;

  class JsonLike
  {
  public:
    virtual ~JsonLike() = default;

    virtual bool isEmpty() const = 0;

    virtual operator RealType() const = 0;
    virtual operator StringType() const = 0;
    virtual operator IntegerType() const = 0;

    virtual void operator=(const RealType& value) = 0;
    virtual void operator=(const StringType& value) = 0;
    virtual void operator=(const IntegerType& value) = 0;

    virtual JsonProxy operator[](const std::string& key) const = 0;
  };

  // For not having to directly deal with `std::unique_ptr<JsonLike>`
  class JsonProxy
  {
  public:
    JsonProxy(std::unique_ptr<JsonLike> json)
      : _json(std::move(json))
    {
      if (!_json)
        throw std::runtime_error("Parameter `json` is a nullptr");
    }

    JsonProxy operator[](const std::string& key)
    {
      return (*_json)[key];
    }

    // For accessing `JsonLike` members
    JsonLike* operator->()
    {
      return _json.get();
    }

    // For implicit conversion
    operator JsonLike& ()
    {
      return *_json;
    }

    // For implicit conversion
    operator const JsonLike& () const
    {
      return *_json;
    }

    template<class T>
    void operator=(const T& value)
    {
      *_json = value;
    }

    template<class T>
    operator T() const
    {
      return getValue<T>();
    }

    template<class T>
    T getValue() const
    {
      if (_json->isEmpty())
        throw std::runtime_error("Expected this `JsonProxy` to reference a json that contains a value");

      try
      {
        return static_cast<T>(*_json);
      }
      catch (const std::exception& exc)
      {
        // Catching and re-throwing here makes it easier to navigate the stack-trace in case of the exception
        std::string msg = "Cannot convert the json value held by this `JsonProxy` to the desired type because: ";
        msg += std::string(exc.what());
        throw std::runtime_error(msg);
      }
    }

  private:
    std::unique_ptr<JsonLike> _json;
  };

  class Group
  {
  public:
    Group(std::string name)
      : _name{ std::move(name) }
    {}

    virtual ~Group() = default;

  public:
    const std::string& getName() const noexcept
    {
      return _name;
    }

    virtual void operator<<(const JsonLike& node) = 0;
    virtual void operator>>(JsonLike& node) const = 0;

  private:
    std::string _name;
  };

  template<class>
  struct less;

  template<template<typename> class smart_ptr>
  struct less<smart_ptr<Group>>
  {
    bool operator()(const smart_ptr<Group>& lhs, const smart_ptr<Group>& rhs) const noexcept
    {
      return lhs->getName() < rhs->getName();
    }
  };

  class Configuration : public Group
  {
  public:
    Configuration(std::string name)
      : Group(std::move(name))
      , _groups()
    {}

    bool contains(std::string& name) const
    {
      return findByName(name) != _groups.cend();
    }

    bool insert(std::shared_ptr<Group> group, bool silent = false)
    {
      auto [other, success] = _groups.insert(std::move(group));

      if (!success && !silent)
        throw std::runtime_error("Unable to insert `group` with name \"" + (*other)->getName()
          + "\" because another group with the same name already exists");

      return success;
    }

    std::shared_ptr<Group> remove(const std::string& name, bool silent = false)
    {
      std::shared_ptr<Group> group;

      if (auto iter = findByName(name); iter != _groups.cend())
      {
        group = *iter;
        _groups.erase(iter);
      }
      else if (!silent)
        throw std::runtime_error("Unable to remove group by name \"" + name + "\" because it does not exist");

      return group;
    }

    template<class GroupType>
    std::shared_ptr<GroupType> getTyped(const std::string& name, bool silent = false) const
    {
      std::shared_ptr<GroupType> typedGroup;

      if (std::shared_ptr<Group> group = get(name, silent); group)
        typedGroup = std::dynamic_pointer_cast<GroupType>(std::move(group));

      if (!typedGroup && !silent)
        throw std::runtime_error("Unable to get group by name \"" + name + "\" in the specified type");

      return typedGroup;
    }

    std::shared_ptr<Group> get(const std::string& name, bool silent = false) const
    {
      std::shared_ptr<Group> group;

      if (auto iter = findByName(name); iter != _groups.cend())
        group = *iter;
      else if (!silent)
        throw std::runtime_error("Cannot get group by name \"" + name + "\" because it does not exist");

      return group;
    }

    void operator<<(const JsonLike& json) override final
    {
      JsonProxy thisJson = json[getName()];

      if (thisJson->isEmpty())
        throw std::runtime_error("Expected `json[" + getName() + "]` to contain a value");

      for (auto& group : _groups)
      {
        JsonProxy groupJson = thisJson[group->getName()];

        if (groupJson->isEmpty())
          throw std::runtime_error("Expected `json[" + getName() + "][" + group->getName() + "]` to contain a value");

        *group << groupJson;
      }
    }

    void operator>>(JsonLike& json) const override final
    {
      JsonProxy thisJson = json[getName()];

      for (auto& group : _groups)
      {
        JsonProxy groupJson = thisJson[group->getName()];

        *group >> groupJson;
      }
    }

    void operator>>(JsonLike&& temporaryJsonWrapper) const
    {
      this->operator>>(temporaryJsonWrapper);
    }

  protected:
    std::set<std::shared_ptr<Group>>::const_iterator findByName(const std::string& name) const
    {
      auto pred = [&name](const auto& group) { return group->getName() == name; };
      return std::find_if(_groups.cbegin(), _groups.cend(), pred);
    }

  private:
    std::set<std::shared_ptr<Group>, less<std::shared_ptr<Group>>> _groups;
  };
} // safeconfig