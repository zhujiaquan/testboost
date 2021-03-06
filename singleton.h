#include <boost/smart_ptr.hpp>
#include <boost/noncopyable.hpp>

class DbMgr : public boost::noncopyable
{
public:
	static  boost::shared_ptr<DbMgr> GetInstance()
	{   
		if(!_instance)
		{
			_instance.reset(new DbMgr());
			_instance->Init();
		}
		return _instance;
	} 
	void Init()
	{

	}
	void Destroy()
	{

	}
	~DbMgr()
	{
		std::cout << "~DbMgr()" << std::endl;
	}
private:
	DbMgr(){}
private:
	static boost::shared_ptr<DbMgr> _instance;
};

//初始化静态成员变量
boost::shared_ptr<DbMgr> DbMgr::_instance = boost::shared_ptr<DbMgr>();