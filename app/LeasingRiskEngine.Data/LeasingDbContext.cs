using Microsoft.EntityFrameworkCore;

namespace LeasingRiskEngine.Data;

public sealed class LeasingDbContext : DbContext
{
    public LeasingDbContext(DbContextOptions<LeasingDbContext> options)
        : base(options) { }

    public DbSet<Customer>            Customers => Set<Customer>();
    public DbSet<ContractRecord>      Contracts => Set<ContractRecord>();
    public DbSet<SimulationRunRecord> Runs      => Set<SimulationRunRecord>();

    protected override void OnModelCreating(ModelBuilder model)
    {
        model.Entity<Customer>(e =>
        {
            e.ToTable("Customers");
            e.Property(x => x.Name).HasMaxLength(200).IsRequired();
            e.HasIndex(x => x.Name).IsUnique();
        });

        model.Entity<ContractRecord>(e =>
        {
            e.ToTable("Contracts");
            e.Property(x => x.Number).HasMaxLength(64).IsRequired();
            e.HasIndex(x => x.Number).IsUnique();
            e.HasOne(x => x.Customer)
             .WithMany(x => x.Contracts)
             .HasForeignKey(x => x.CustomerId)
             .OnDelete(DeleteBehavior.Cascade);
        });

        model.Entity<SimulationRunRecord>(e =>
        {
            e.ToTable("SimulationRuns");
            e.Property(x => x.Kind).HasMaxLength(32).IsRequired();
            e.Property(x => x.Scenario).HasMaxLength(200).IsRequired();
            e.Property(x => x.Scheme).HasMaxLength(32).IsRequired();
            e.Ignore(x => x.CreatedLocal);
            e.HasIndex(x => x.CreatedUtc);
        });
    }
}
