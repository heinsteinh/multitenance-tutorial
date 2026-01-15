#include "services/mapper.hpp"

namespace services::mapper {

multitenant::v1::User to_proto(const services::UserModel &user) {
  multitenant::v1::User proto;
  proto.set_id(user.id);
  proto.set_username(user.username);
  proto.set_email(user.email);
  proto.set_role(user.role);
  proto.set_active(user.active);
  return proto;
}

services::UserModel from_proto(const multitenant::v1::User &proto) {
  services::UserModel user;
  user.id = proto.id();
  user.username = proto.username();
  user.email = proto.email();
  user.role = proto.role();
  user.active = proto.active();
  return user;
}

services::CreateUserDto
from_proto(const multitenant::v1::CreateUserRequest &proto) {
  services::CreateUserDto dto;
  // tenant_id comes from request metadata, set by handler
  dto.username = proto.username();
  dto.email = proto.email();
  dto.password_hash = proto.password(); // Password would be hashed in service layer
  dto.role = proto.role();
  dto.active = true;
  return dto;
}

services::UpdateUserDto
from_proto(const multitenant::v1::UpdateUserRequest &proto) {
  services::UpdateUserDto dto;
  if (proto.has_username()) {
    dto.username = proto.username();
  }
  if (proto.has_email()) {
    dto.email = proto.email();
  }
  if (proto.has_password()) {
    dto.password = proto.password();
  }
  if (proto.has_role()) {
    dto.role = proto.role();
  }
  if (proto.has_active()) {
    dto.active = proto.active();
  }
  return dto;
}

multitenant::v1::Tenant to_proto(const services::TenantModel &tenant) {
  multitenant::v1::Tenant proto;
  proto.set_id(tenant.id);
  proto.set_tenant_id(tenant.tenant_id);
  proto.set_name(tenant.name);
  proto.set_plan(tenant.plan);
  proto.set_active(tenant.active);
  return proto;
}

services::TenantModel from_proto(const multitenant::v1::Tenant &proto) {
  services::TenantModel tenant;
  tenant.id = proto.id();
  tenant.tenant_id = proto.tenant_id();
  tenant.name = proto.name();
  tenant.plan = proto.plan();
  tenant.active = proto.active();
  return tenant;
}

services::CreateTenantDto
from_proto(const multitenant::v1::CreateTenantRequest &proto) {
  services::CreateTenantDto dto;
  dto.tenant_id = proto.tenant_id();
  dto.name = proto.name();
  dto.plan = proto.plan();
  dto.active = true;
  return dto;
}

services::UpdateTenantDto
from_proto(const multitenant::v1::UpdateTenantRequest &proto) {
  services::UpdateTenantDto dto;
  if (proto.has_name()) {
    dto.name = proto.name();
  }
  if (proto.has_plan()) {
    dto.plan = proto.plan();
  }
  if (proto.has_active()) {
    dto.active = proto.active();
  }
  return dto;
}

} // namespace services::mapper
