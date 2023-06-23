
#include "safeconfig.h"
#include "nlohmann_json.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <utility>

using safeconfig::Property;
using safeconfig::ChoiceProperty;
using safeconfig::NumericProperty;

using safeconfig::Group;
using safeconfig::Configuration;

using safeconfig::JsonLike;
using Json = nlohmann::json;

class NLohmannJsonWrapper : public JsonLike
{
public:
  NLohmannJsonWrapper(nlohmann::json& json)
    : _json(json)
  {
  }

  safeconfig::JsonProxy operator[](const std::string& key) const override
  {
    return { std::make_unique<NLohmannJsonWrapper>(_json[key]) };
  }

  bool isEmpty() const override
  {
    return _json.is_null();
  }

  operator int() const override
  {
    return static_cast<int>(_json);
  }

  operator double() const override
  {
    return static_cast<double>(_json);
  }

  operator std::string() const override
  {
    return static_cast<std::string>(_json);
  }

  void operator=(const int& value) override
  {
    _json = value;
  }

  void operator=(const double& value) override
  {
    _json = value;
  }

  void operator=(const std::string& value) override
  {
    _json = value;
  }

private:
  Json& _json;
};

class Logging : public Group
{
  static inline const std::vector<std::string> defaultLoggingLevelChoices = { "trace", "debug", "info", "warn", "err", "critical", "off" };
  static constexpr std::pair<int, int> defaultFlushPeriodInSecondsRange = { 0, 9000 };

public:
  Logging(std::string name)
    : Group(std::move(name))
    , _level(std::make_unique<ChoiceProperty>("level", defaultLoggingLevelChoices))
    , _period(std::make_unique<NumericProperty>("flushPeriodInSeconds", defaultFlushPeriodInSecondsRange))
  {
  }

  Logging(Logging&&) = default;
  Logging& operator=(Logging&&) = default;

  auto getLoggingLevel() const
  {
    return _level->getValue<std::string>();
  }

  void setLoggingLevel(const std::string& level)
  {
    _level->setValue(level);
  }

  auto getFlushPeriodInSeconds() const
  {
    return _period->getValue<int>();
  }

  void setFlushPeriodInSeconds(const int& level)
  {
    _period->setValue(level);
  }

  void setLoggingLevelChoices(const std::vector<std::string>& choices)
  {
    _level->setConstraint(std::make_unique<ChoiceProperty::ConstraintType>(choices));
  }

  void operator<<(const JsonLike& json) override
  {
    _level->setValue<std::string>(json[_level->getName()]);
    _period->setValue<int>(json[_period->getName()]);
  }

  void operator>>(JsonLike& json) const override
  {
    json[_level->getName()] = _level->getValue<std::string>();
    json[_period->getName()] = _period->getValue<int>();
  }

protected:
  std::unique_ptr<Property> _level;
  std::unique_ptr<Property> _period;
};

class MyConfiguration : public Configuration
{
public:
  MyConfiguration(std::string name)
    : Configuration(std::move(name))
  {
    // Insert groups as needed
    insert(std::make_shared<Logging>("logging"));
  }

  auto getLogging() const
  {
    return getTyped<Logging>("logging");
  }
};


#define ASSERT_THROWS(explain, expr) {                              \
  std::cout << "TEST  : " explain << std::endl;                     \
  bool wasCaught = false;                                           \
  try {                                                             \
    expr;                                                           \
  }                                                                 \
  catch (const std::exception& exc) {                               \
    std::cout << "EXPR  : " #expr << std::endl;                     \
    std::cout << "CATCH : " + std::string(exc.what());              \
    std::cout << " [PASS]" << std::endl << std::endl;               \
    wasCaught = true;                                               \
  }                                                                 \
  assert(wasCaught);                                                \
  }

#define ASSERT_POSTCOND(explain, expr, postCond) {                  \
  std::cout << "TEST  : " explain << std::endl;                     \
  std::cout << "EXPR  : " #expr << std::endl;                       \
  expr;                                                             \
  assert(postCond);                                                 \
  std::cout << "POST  : " #postCond " [PASS]" << std::endl;         \
  std::cout << std::endl;                                           \
  }

int main()
{
  using namespace safeconfig;


  std::cout << "\n*** TEST: NumericProperty ***\n" << std::endl;

  NumericProperty numericProperty("Numeric Property", std::pair<int, int>{ 0, 10 } /*valid value range*/);
  //ASSERT_THROWS("Using an unknown value type in a numeric property", numericProperty.getValue<const char*>()); // static_assert
  ASSERT_THROWS("Using a wrong value type to read the value of a numeric property", numericProperty.getValue<std::string>());
  ASSERT_POSTCOND("Default value of numeric property is valid", int value = numericProperty.getValue<int>(), value == 0);

  numericProperty.setConstraint(1, 10);

  ASSERT_THROWS("Changed numeric property constraint. Old property value is invalid", numericProperty.getValue<int>());
  ASSERT_POSTCOND("Setting a new valid numeric property value", numericProperty.setValue(1), numericProperty.getValue<int>() == 1);

  std::cout << "\n*** TEST: ChoiceProperty ***\n" << std::endl;

  ChoiceProperty choiceProperty("Choice Property", std::vector<std::string>{"debug", "info", "crit"});
  //ASSERT_THROWS("Using an unknown value type in a choice property", choiceProperty.getValue<const char*>()); // static_assert
  ASSERT_THROWS("Setting an invalid choice property value", choiceProperty.setValue<std::string>("trace"));
  ASSERT_POSTCOND("Setting a valid choice property value", choiceProperty.setValue<std::string>("info"), choiceProperty.getValue<std::string>() == "info");

  std::vector<int> choicePropertyChoices = { 1, 2, 3 };
  choiceProperty.setConstraint(choicePropertyChoices);

  ASSERT_THROWS("Choice property constraint changed. Old value is invalid", choiceProperty.getValue<std::string>());
  ASSERT_THROWS("Setting a choice property value of a wrong type", choiceProperty.setValue<std::string>("debug"));
  ASSERT_THROWS("Setting an invalid choice property value", choiceProperty.setValue(0));

  ASSERT_POSTCOND("Setting a valid choice property value", choiceProperty.setValue(1), choiceProperty.getValue<int>() == 1);
  ASSERT_THROWS("Getting a choice property value of a wrong type", choiceProperty.getValue<std::string>());
  ASSERT_POSTCOND("Getting the choice property value of correct type", int value = choiceProperty.getValue<int>(), value == 1);

  std::cout << "\n*** TEST: Group ***\n" << std::endl;

  MyConfiguration myConfig("myConfig");

  std::shared_ptr<Logging> logging = myConfig.getLogging();

  ASSERT_THROWS("Getting an invalid (default) logging level", logging->getLoggingLevel());
  ASSERT_THROWS("Setting an invalid logging level", logging->setLoggingLevel("offf"));
  ASSERT_POSTCOND("Setting a valid logging level", logging->setLoggingLevel("off"), logging->getLoggingLevel() == "off");

  ASSERT_POSTCOND("Getting a valid (default) flush period", int value = logging->getFlushPeriodInSeconds(), value == 0);
  ASSERT_THROWS("Setting an invalid flush period", logging->setFlushPeriodInSeconds(-1));
  ASSERT_POSTCOND("Setting a valid flush period", logging->setFlushPeriodInSeconds(60), logging->getFlushPeriodInSeconds() == 60);

  std::vector<std::string> loggingLevelChoices = { "debug", "info" };
  logging->setLoggingLevelChoices(loggingLevelChoices);

  ASSERT_THROWS("Logging level choices changed. Old logging level value is invalid", logging->getLoggingLevel());
  ASSERT_POSTCOND("Setting a new valid logging level", logging->setLoggingLevel("info"), logging->getLoggingLevel() == "info");

  std::cout << "\n*** TEST: Json input/output ***\n" << std::endl;

  Json inputMyConfigJson;
  Json outputMyConfigJson;

  // Assume this is read from a file
  inputMyConfigJson["myConfiggg"]["logginggg"]["levelll"] = "infooo"; // Bad keys and value 
  inputMyConfigJson["myConfig"]["logging"]["flushPeriodInSeconds"] = -1; // Bad value
  ASSERT_THROWS("Reading an invalid configuration from json", myConfig << NLohmannJsonWrapper(inputMyConfigJson));
  
  inputMyConfigJson["myConfig"]["logginggg"]["levelll"] = "infooo"; // Still wrong keys and value
  ASSERT_THROWS("Reading an invalid configuration from json", myConfig << NLohmannJsonWrapper(inputMyConfigJson));

  inputMyConfigJson["myConfig"]["logging"]["levelll"] = "infooo"; // Wrong key and value
  ASSERT_THROWS("Reading an invalid configuration from json", myConfig << NLohmannJsonWrapper(inputMyConfigJson));

  inputMyConfigJson["myConfig"]["logging"]["levelll"] = "infooo"; // Wrong key and value
  ASSERT_THROWS("Reading an invalid configuration from json", myConfig << NLohmannJsonWrapper(inputMyConfigJson));

  inputMyConfigJson["myConfig"]["logging"]["level"] = "infooo"; // Wrong value
  ASSERT_THROWS("Reading an invalid configuration from json", myConfig << NLohmannJsonWrapper(inputMyConfigJson));

  inputMyConfigJson["myConfig"]["logging"]["level"] = "info"; // Good, but flush period is invalid
  ASSERT_THROWS("Reading an invalid configuration from json", myConfig << NLohmannJsonWrapper(inputMyConfigJson));

  inputMyConfigJson["myConfig"]["logging"]["flushPeriodInSeconds"] = 3; // All good

  myConfig << NLohmannJsonWrapper(inputMyConfigJson); // Good
  myConfig >> NLohmannJsonWrapper(outputMyConfigJson); // Good

  std::cout << std::endl;
  std::cout << "outputMyConfigJson:" << std::endl;
  std::cout << outputMyConfigJson << std::endl;
  std::cout << std::endl;
  }