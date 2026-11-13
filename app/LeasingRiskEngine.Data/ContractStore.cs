using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.EntityFrameworkCore;

namespace LeasingRiskEngine.Data;

/// <summary>A contract together with its customer, as the UI sees it.</summary>
public sealed record SavedContract
{
    public string   CustomerName   { get; init; } = "";
    public string   Number         { get; init; } = "";
    public double   Principal      { get; init; }
    public double   DownPayment    { get; init; }
    public double   AnnualRate     { get; init; }
    public int      TermMonths     { get; init; }
    public int      PaymentType    { get; init; }
    public DateTime StartDate      { get; init; }
    public double   MonthlyPayment { get; init; }
    public double   TotalPayments  { get; init; }
    public double   TotalInterest  { get; init; }
    public DateTime SavedUtc       { get; init; }

    public string Title => $"{Number} — {CustomerName}";
}

public interface IContractStore
{
    /// <summary>All saved contracts, most recently saved first.</summary>
    Task<IReadOnlyList<SavedContract>> ListAsync(CancellationToken ct = default);

    /// <summary>
    /// Inserts the contract, or updates it if one with the same number exists.
    /// The customer is looked up by name and created when new.
    /// </summary>
    Task SaveAsync(SavedContract contract, CancellationToken ct = default);

    /// <summary>Returns false if there was no contract with that number.</summary>
    Task<bool> DeleteAsync(string number, CancellationToken ct = default);
}

public sealed class ContractStore : IContractStore
{
    private readonly LeasingDatabase _database;

    public ContractStore(LeasingDatabase database) => _database = database;

    public Task<IReadOnlyList<SavedContract>> ListAsync(CancellationToken ct = default) =>
        _database.RunAsync<IReadOnlyList<SavedContract>>(async (db, token) =>
            await db.Contracts
                .AsNoTracking()
                .OrderByDescending(c => c.SavedUtc)
                .Select(c => new SavedContract
                {
                    CustomerName   = c.Customer.Name,
                    Number         = c.Number,
                    Principal      = c.Principal,
                    DownPayment    = c.DownPayment,
                    AnnualRate     = c.AnnualRate,
                    TermMonths     = c.TermMonths,
                    PaymentType    = c.PaymentType,
                    StartDate      = c.StartDate,
                    MonthlyPayment = c.MonthlyPayment,
                    TotalPayments  = c.TotalPayments,
                    TotalInterest  = c.TotalInterest,
                    SavedUtc       = c.SavedUtc,
                })
                .ToListAsync(token)
                .ConfigureAwait(false), ct);

    public Task SaveAsync(SavedContract contract, CancellationToken ct = default)
    {
        var customerName = contract.CustomerName.Trim();
        var number       = contract.Number.Trim();
        if (customerName.Length == 0)
            throw new ArgumentException("Customer name is required.", nameof(contract));
        if (number.Length == 0)
            throw new ArgumentException("Contract number is required.", nameof(contract));

        return _database.RunAsync(async (db, token) =>
        {
            var now = DateTime.UtcNow;

            var customer = await db.Customers
                .FirstOrDefaultAsync(c => c.Name == customerName, token)
                .ConfigureAwait(false);
            if (customer is null)
            {
                customer = new Customer { Name = customerName, CreatedUtc = now };
                db.Customers.Add(customer);
            }

            var record = await db.Contracts
                .FirstOrDefaultAsync(c => c.Number == number, token)
                .ConfigureAwait(false);
            if (record is null)
            {
                record = new ContractRecord { Number = number };
                db.Contracts.Add(record);
            }

            record.Customer       = customer;
            record.Principal      = contract.Principal;
            record.DownPayment    = contract.DownPayment;
            record.AnnualRate     = contract.AnnualRate;
            record.TermMonths     = contract.TermMonths;
            record.PaymentType    = contract.PaymentType;
            record.StartDate      = contract.StartDate.Date;
            record.MonthlyPayment = contract.MonthlyPayment;
            record.TotalPayments  = contract.TotalPayments;
            record.TotalInterest  = contract.TotalInterest;
            record.SavedUtc       = now;

            await db.SaveChangesAsync(token).ConfigureAwait(false);
            return true;
        }, ct);
    }

    public Task<bool> DeleteAsync(string number, CancellationToken ct = default) =>
        _database.RunAsync(async (db, token) =>
            await db.Contracts
                .Where(c => c.Number == number)
                .ExecuteDeleteAsync(token)
                .ConfigureAwait(false) > 0, ct);
}
