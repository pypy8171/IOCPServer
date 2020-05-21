#pragma once

#define SINGLE(className) public: \
						static className* GetInst() \
						{\
							static className mgr;\
							return &mgr;\
						}\
						private:\
							className();\
							~className();