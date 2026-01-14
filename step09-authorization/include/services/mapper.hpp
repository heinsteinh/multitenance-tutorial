#pragma once

#include "dto.hpp"

#include "tenant.pb.h"
#include "user.pb.h"

namespace services::mapper {

multitenant::v1::User to_proto(const services::UserModel &user);
services::UserModel from_proto(const multitenant::v1::User &proto);
services::CreateUserDto
from_proto(const multitenant::v1::CreateUserRequest &proto);
services::UpdateUserDto
from_proto(const multitenant::v1::UpdateUserRequest &proto);

multitenant::v1::Tenant to_proto(const services::TenantModel &tenant);
services::TenantModel from_proto(const multitenant::v1::Tenant &proto);
services::CreateTenantDto
from_proto(const multitenant::v1::CreateTenantRequest &proto);
services::UpdateTenantDto
from_proto(const multitenant::v1::UpdateTenantRequest &proto);

} // namespace services::mapper
