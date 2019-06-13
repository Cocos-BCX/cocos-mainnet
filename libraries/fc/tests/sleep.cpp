#include <fc/thread/thread.hpp>
#include <iostream>
using namespace fc;
int main( int argc, char** argv )
{/*
  fc::thread test("test");
  auto result = test.async( [=]() {
      while( true )
      {
        std::cout << "in while"<<std::endl;
        fc::usleep( fc::microseconds(100000) );
      }
  });
  char c;
  std::cin >> c;
  test.quit();
  return 0;

*/
   bool called1 = false;
    bool called2 = false;

    fc::thread thread1("my1");
    fc::thread thread2("my2");
    auto future1 = thread1.async([&called1]{called1 = true; int i=10; while(i--)
                                                                    {
                                                                        fc::yield();
                                                                        sleep(1);
                                                                        printf("future1:%d\r\n",i);
                                                                    }
                                                                    });
    auto future2 = thread1.async([&called2]{called2 = true;int i=10; while(i--)
                                                                    {
                                                                        fc::yield();
                                                                    
                                                                        printf("future2:%d\r\n",i);
                                                                    }
    });
  char c;
  std::cin >> c;
  //thread1.quit();
  //thread2.quit(); //nico 单线程实例多异步处理时，主线程结束时，子线程不会结束，多异步中有延时函数(sleep)，以最大延时为准，并且无法使用quit退出
  //std::cin >> c;
    
}
