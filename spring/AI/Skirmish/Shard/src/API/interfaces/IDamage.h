#ifndef IDAMAGE_H
#define IDAMAGE_H

#include "../Position.h"
#include <memory>

class IDamage {
public:
	typedef std::shared_ptr<IDamage> Ptr;

	virtual ~IDamage(){}

	virtual float Damage()=0;
	virtual Position Direction()=0;
	virtual std::string DamageType()=0;
	virtual std::string WeaponType()=0;
	virtual IUnit* Attacker()=0;
	virtual std::vector<std::string> Effects()=0;
};

#endif /* IDAMAGE_H */

