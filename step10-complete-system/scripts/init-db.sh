#!/bin/bash
# Database initialization script for step10 complete system

set -e

DB_PATH="${DATABASE_PATH:-/app/data/multitenant.db}"
DB_DIR=$(dirname "$DB_PATH")

echo "Initializing database at: $DB_PATH"

# Create data directory if it doesn't exist
if [ ! -d "$DB_DIR" ]; then
    echo "Creating data directory: $DB_DIR"
    mkdir -p "$DB_DIR"
fi

# Check if database already exists
if [ -f "$DB_PATH" ]; then
    echo "Database already exists. Skipping initialization."
    exit 0
fi

echo "Database will be initialized on first server startup."
echo "The server will create the following tables:"
echo "  - schema_version (for tracking schema migrations)"
echo "  - tenants (multi-tenant organization data)"
echo "  - users (user accounts per tenant)"
echo "  - roles (RBAC role definitions)"
echo "  - role_permissions (permissions per role)"
echo "  - user_roles (user-to-role assignments)"

echo ""
echo "Default data that will be seeded:"
echo "  - Demo tenant (tenant_id: 'demo')"
echo "  - Admin role with full permissions"
echo "  - User role with read-only permissions"

echo ""
echo "Database initialization complete."
